/*	$OpenBSD: if_otus.c,v 1.69 2021/02/25 02:48:20 dlg Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Driver for Atheros AR9001U chipset.
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/endian.h>

#include <machine/intr.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_amrr.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbdevs.h>

#include <dev/usb/if_otusreg.h>

#ifdef OTUS_DEBUG
#define DPRINTF(x)	do { if (otus_debug) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (otus_debug >= (n)) printf x; } while (0)
int	otus_debug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

void
otus_attachhook(struct device *self)
{
	struct otus_softc *sc = (struct otus_softc *)self;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	usb_device_request_t req;
	uint32_t in, out;
	int error;

	error = otus_load_firmware(sc, "otus-init", AR_FW_INIT_ADDR);
	if (error != 0) {
		printf("%s: could not load %s firmware\n", sc->sc_dev.dv_xname,
		    "init");
		return;
	}

	usbd_delay_ms(sc->sc_udev, 1000);

	error = otus_load_firmware(sc, "otus-main", AR_FW_MAIN_ADDR);
	if (error != 0) {
		printf("%s: could not load %s firmware\n", sc->sc_dev.dv_xname,
		    "main");
		return;
	}

	/* Tell device that firmware transfer is complete. */
	req.bmRequestType = UT_WRITE_VENDOR_DEVICE;
	req.bRequest = AR_FW_DOWNLOAD_COMPLETE;
	USETW(req.wValue, 0);
	USETW(req.wIndex, 0);
	USETW(req.wLength, 0);
	if (usbd_do_request(sc->sc_udev, &req, NULL) != 0) {
		printf("%s: firmware initialization failed\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	/* Send an ECHO command to check that everything is settled. */
	in = 0xbadc0ffe;
	if (otus_cmd(sc, AR_CMD_ECHO, &in, sizeof in, &out) != 0) {
		printf("%s: echo command failed\n", sc->sc_dev.dv_xname);
		return;
	}
	if (in != out) {
		printf("%s: echo reply mismatch: 0x%08x!=0x%08x\n",
		    sc->sc_dev.dv_xname, in, out);
		return;
	}

	/* Read entire EEPROM. */
	if (otus_read_eeprom(sc) != 0) {
		printf("%s: could not read EEPROM\n", sc->sc_dev.dv_xname);
		return;
	}

	sc->txmask = sc->eeprom.baseEepHeader.txMask;
	sc->rxmask = sc->eeprom.baseEepHeader.rxMask;
	sc->capflags = sc->eeprom.baseEepHeader.opCapFlags;
	IEEE80211_ADDR_COPY(ic->ic_myaddr, sc->eeprom.baseEepHeader.macAddr);
	sc->sc_led_newstate = otus_led_newstate_type3;	/* XXX */

	printf("%s: MAC/BBP AR9170, RF AR%X, MIMO %dT%dR, address %s\n",
	    sc->sc_dev.dv_xname,
	    (sc->capflags & AR5416_OPFLAGS_11A) ? 0x9104 : ((sc->txmask ==
	    0x5) ? 0x9102 : 0x9101),
	    (sc->txmask == 0x5) ? 2 : 1,
	    (sc->rxmask == 0x5) ? 2 : 1, ether_sprintf(ic->ic_myaddr));

	ic->ic_phytype = IEEE80211_T_OFDM;	/* not only, but not used */
	ic->ic_opmode = IEEE80211_M_STA;	/* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	if (sc->eeprom.baseEepHeader.opCapFlags & AR5416_OPFLAGS_11G) {
		/* Set supported .11b and .11g rates. */
		ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
		ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;
	}
	if (sc->eeprom.baseEepHeader.opCapFlags & AR5416_OPFLAGS_11A) {
		/* Set supported .11a rates. */
		ic->ic_sup_rates[IEEE80211_MODE_11A] = ieee80211_std_rateset_11a;
	}

	/* Build the list of supported channels. */
	otus_get_chanlist(sc);

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = otus_ioctl;
	ifp->if_start = otus_start;
	ifp->if_watchdog = otus_watchdog;
	memcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	ic->ic_node_alloc = otus_node_alloc;
	ic->ic_newassoc = otus_newassoc;
	ic->ic_updateslot = otus_updateslot;
	ic->ic_updateedca = otus_updateedca;
#ifdef notyet
	ic->ic_set_key = otus_set_key;
	ic->ic_delete_key = otus_delete_key;
#endif
	/* Override state transition machine. */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = otus_newstate;
	ieee80211_media_init(ifp, otus_media_change, ieee80211_media_status);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + IEEE80211_RADIOTAP_HDRLEN);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(OTUS_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(OTUS_TX_RADIOTAP_PRESENT);
#endif
}

/* ARGSUSED */
void
otus_intr(struct usbd_xfer *xfer, void *priv, usbd_status status)
{
#if 0
	struct otus_softc *sc = priv;
	int len;

	/*
	 * The Rx intr pipe is unused with current firmware.  Notifications
	 * and replies to commands are sent through the Rx bulk pipe instead
	 * (with a magic PLCP header.)
	 */
	if (__predict_false(status != USBD_NORMAL_COMPLETION)) {
		DPRINTF(("intr status=%d\n", status));
		if (status == USBD_STALLED)
			usbd_clear_endpoint_stall_async(sc->cmd_rx_pipe);
		return;
	}
	usbd_get_xfer_status(xfer, NULL, NULL, &len, NULL);

	otus_cmd_rxeof(sc, sc->ibuf, len);
#endif
}

void
otus_cmd_rxeof(struct otus_softc *sc, uint8_t *buf, int len)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct otus_tx_cmd *cmd;
	struct ar_cmd_hdr *hdr;
	int s;

	if (__predict_false(len < sizeof(*hdr))) {
		DPRINTF(("cmd too small %d\n", len));
		return;
	}
	hdr = (struct ar_cmd_hdr *)buf;
	if (__predict_false(sizeof(*hdr) + hdr->len > len ||
	    sizeof(*hdr) + hdr->len > 64)) {
		DPRINTF(("cmd too large %d\n", hdr->len));
		return;
	}

	if ((hdr->code & 0xc0) != 0xc0) {
		DPRINTFN(2,
		    ("received reply code=0x%02x len=%d token=%d\n", hdr->code,
		     hdr->len, hdr->token));
		cmd = &sc->tx_cmd;
		if (__predict_false(hdr->token != cmd->token))
			return;
		/* Copy answer into caller's supplied buffer. */
		if (cmd->odata != NULL)
			memcpy(cmd->odata, &hdr[1], hdr->len);
		cmd->done = 1;
		wakeup(cmd);
		return;
	}

	/* Received unsolicited notification. */
	DPRINTF(("received notification code=0x%02x len=%d\n", hdr->code,
	    hdr->len));
	switch (hdr->code & 0x3f) {
	case AR_EVT_BEACON:
		break;
	case AR_EVT_TX_COMP: {
		struct ar_evt_tx_comp *tx = (struct ar_evt_tx_comp *)&hdr[1];
		struct ieee80211_node *ni;
		struct otus_node *on;

		DPRINTF(("tx completed %s status=%d phy=0x%x\n",
		    ether_sprintf(tx->macaddr), letoh16(tx->status),
		    letoh32(tx->phy)));
		s = splnet();
#ifdef notyet
#ifndef IEEE80211_STA_ONLY
		if (ic->ic_opmode != IEEE80211_M_STA) {
			ni = ieee80211_find_node(ic, tx->macaddr);
			if (__predict_false(ni == NULL)) {
				splx(s);
				break;
			}
		} else
#endif
#endif
			ni = ic->ic_bss;
		/* Update rate control statistics. */
		on = (void *)ni;
		/* NB: we do not set the TX_MAC_RATE_PROBING flag. */
		if (__predict_true(tx->status != 0))
			on->amn.amn_retrycnt++;
		splx(s);
		break;
	}
	case AR_EVT_TBTT:
		break;
	}
}

void
otus_start(struct ifnet *ifp)
{
	struct otus_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct mbuf *m;

	if (!(ifp->if_flags & IFF_RUNNING) || ifq_is_oactive(&ifp->if_snd))
		return;

	for (;;) {
		if (sc->tx_queued >= OTUS_TX_DATA_LIST_COUNT) {
			ifq_set_oactive(&ifp->if_snd);
			break;
		}
		/* Send pending management frames first. */
		m = mq_dequeue(&ic->ic_mgtq);
		if (m != NULL) {
			ni = m->m_pkthdr.ph_cookie;
			goto sendit;
		}
		if (ic->ic_state != IEEE80211_S_RUN)
			break;

		/* Encapsulate and send data frames. */
		m = ifq_dequeue(&ifp->if_snd);
		if (m == NULL)
			break;
#if NBPFILTER > 0
		if (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		if ((m = ieee80211_encap(ifp, m, &ni)) == NULL)
			continue;
sendit:
#if NBPFILTER > 0
		if (ic->ic_rawbpf != NULL)
			bpf_mtap(ic->ic_rawbpf, m, BPF_DIRECTION_OUT);
#endif
		if (otus_tx(sc, m, ni) != 0) {
			ieee80211_release_node(ic, ni);
			ifp->if_oerrors++;
			continue;
		}

		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

#ifdef notyet
int
otus_set_key(struct ieee80211com *ic, struct ieee80211_node *ni,
    struct ieee80211_key *k)
{
	struct otus_softc *sc = ic->ic_softc;
	struct otus_cmd_key cmd;

	/* Defer setting of WEP keys until interface is brought up. */
	if ((ic->ic_if.if_flags & (IFF_UP | IFF_RUNNING)) !=
	    (IFF_UP | IFF_RUNNING))
		return 0;

	/* Do it in a process context. */
	cmd.key = *k;
	cmd.ni = *ni;
	otus_do_async(sc, otus_set_key_cb, &cmd, sizeof cmd);
	sc->sc_key_tasks++
	return EBUSY;
}

#endif

void
otus_calibrate_to(void *arg)
{
	struct otus_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	int s;

	if (usbd_is_dying(sc->sc_udev))
		return;

	usbd_ref_incr(sc->sc_udev);

	s = splnet();
	ni = ic->ic_bss;
	ieee80211_amrr_choose(&sc->amrr, ni, &((struct otus_node *)ni)->amn);
	splx(s);

	if (!usbd_is_dying(sc->sc_udev))
		timeout_add_sec(&sc->calib_to, 1);

	usbd_ref_decr(sc->sc_udev);
}
