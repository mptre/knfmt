static inline unsigned long
bitmap_find_next_zero_area(unsigned long *map,
			   unsigned long size,
			   unsigned long start,
			   unsigned int nr,
			   unsigned long align_mask)
{
}

static inline void
bitmap_clear(void *p, int b, u_int n)
{
	u_int end = b + n;

	for (; b < end; b++)
		__clear_bit(b, p);
}

/**
 * dwc2_periodic_channel_available() - Checks that a channel is available for a
 * periodic transfer
 *
 * @hsotg: The HCD state structure for the DWC OTG controller
 *
 * Return: 0 if successful, negative error code otherwise
 */
STATIC int dwc2_periodic_channel_available(struct dwc2_hsotg *hsotg)
{
	/*
	 * Currently assuming that there is a dedicated host channel for
	 * each periodic transaction plus at least one host channel for
	 * non-periodic transactions
	 */
	int status;
	int num_channels;

	num_channels = hsotg->params.host_channels;
	if ((hsotg->periodic_channels + hsotg->non_periodic_channels <
	     num_channels) && (hsotg->periodic_channels < num_channels - 1)) {
		status = 0;
	} else {
		dev_dbg(hsotg->dev,
			"%s: Total channels: %d, Periodic: %d, Non-periodic: %d\n",
			__func__, num_channels,
			hsotg->periodic_channels, hsotg->non_periodic_channels);
		status = -ENOSPC;
	}

	return status;
}

/**
 * dwc2_check_periodic_bandwidth() - Checks that there is sufficient bandwidth
 * for the specified QH in the periodic schedule
 *
 * @hsotg: The HCD state structure for the DWC OTG controller
 * @qh:    QH containing periodic bandwidth required
 *
 * Return: 0 if successful, negative error code otherwise
 *
 * For simplicity, this calculation assumes that all the transfers in the
 * periodic schedule may occur in the same (micro)frame
 */
STATIC int dwc2_check_periodic_bandwidth(struct dwc2_hsotg *hsotg,
					 struct dwc2_qh *qh)
{
}

/**
 * dwc2_get_ls_map() - Get the map used for the given qh
 *
 * @hsotg: The HCD state structure for the DWC OTG controller.
 * @qh:    QH for the periodic transfer.
 *
 * We'll always get the periodic map out of our TT.  Note that even if we're
 * running the host straight in low speed / full speed mode it appears as if
 * a TT is allocated for us, so we'll use it.  If that ever changes we can
 * add logic here to get a map out of "hsotg" if !qh->do_split.
 *
 * Returns: the map or NULL if a map couldn't be found.
 */
static unsigned long *dwc2_get_ls_map(struct dwc2_hsotg *hsotg,
				      struct dwc2_qh *qh)
{
}

#ifdef DWC2_PRINT_SCHEDULE
/*
 * cat_printf() - A printf() + strcat() helper
 *
 * This is useful for concatenating a bunch of strings where each string is
 * constructed using printf.
 *
 * @buf:   The destination buffer; will be updated to point after the printed
 *         data.
 * @size:  The number of bytes in the buffer (includes space for '\0').
 * @fmt:   The format for printf.
 * @...:   The args for printf.
 */
static __printf(3, 4)
void cat_printf(char **buf, size_t *size, const char *fmt, ...)
{
	if (i >= *size) {
	} else {
	}
}

/*
 * pmap_print() - Print the given periodic map
 *
 * Will attempt to print out the periodic schedule.
 *
 * @map:             See pmap_schedule().
 * @bits_per_period: See pmap_schedule().
 * @periods_in_map:  See pmap_schedule().
 * @period_name:     The name of 1 period, like "uFrame"
 * @units:           The name of the units, like "us".
 * @print_fn:        The function to call for printing.
 * @print_data:      Opaque data to pass to the print function.
 */
static void pmap_print(unsigned long *map, int bits_per_period,
		       int periods_in_map, const char *period_name,
		       const char *units,
		       void (*print_fn)(const char *str, void *data),
		       void *print_data)
{
}

struct dwc2_qh_print_data {
	struct dwc2_hsotg *hsotg;
	struct dwc2_qh *qh;
};

/**
 * dwc2_qh_print() - Helper function for dwc2_qh_schedule_print()
 *
 * @str:  The string to print
 * @data: A pointer to a struct dwc2_qh_print_data
 */
static void dwc2_qh_print(const char *str, void *data)
{
}

/**
 * dwc2_qh_schedule_print() - Print the periodic schedule
 *
 * @hsotg: The HCD state structure for the DWC OTG controller.
 * @qh:    QH to print.
 */
static void dwc2_qh_schedule_print(struct dwc2_hsotg *hsotg,
				   struct dwc2_qh *qh)
{
}
#else
static inline void dwc2_qh_schedule_print(struct dwc2_hsotg *hsotg,
					  struct dwc2_qh *qh) {};
#endif

static int dwc2_uframe_schedule_split(struct dwc2_hsotg *hsotg,
				      struct dwc2_qh *qh)
{
	while (ls_search_slice < DWC2_LS_SCHEDULE_SLICES) {
		/*
		 * For now, skip OUT xfers where first xfer is partial
		 *
		 * Main dwc2 code assumes:
		 * - INT transfers never get split in two.
		 * - ISOC transfers can always transfer 188 bytes the first
		 *   time.
		 *
		 * Until that code is fixed, try again if the first transfer
		 * couldn't transfer everything.
		 *
		 * This code can be removed if/when the rest of dwc2 handles
		 * the above cases.  Until it's fixed we just won't be able
		 * to schedule quite as tightly.
		 */
		if (!qh->ep_is_in &&
		    (first_data_bytes != min_t(int, 188, bytecount))) {
			dwc2_sch_dbg(hsotg,
				     "QH=%p avoiding broken 1st xfer (%d, %d)\n",
				     qh, first_data_bytes, bytecount);
			if (qh->schedule_low_speed)
				dwc2_ls_pmap_unschedule(hsotg, qh);
			ls_search_slice = (start_s_uframe + 1) *
				DWC2_SLICES_PER_UFRAME;
			continue;
		}

		/*
		 * Everything except ISOC OUT has extra transfers.  Rules are
		 * complicated.  See 11.18.4 Host Split Transaction Scheduling
		 * Requirements bullet 3.
		 */
		if (qh->ep_type == USB_ENDPOINT_XFER_INT) {
		} else {
		}
	}
}

/**
 * dwc2_uframe_schedule_hs - Schedule a QH for a periodic high speed xfer.
 *
 * Basically this just wraps dwc2_hs_pmap_schedule() to provide a clean
 * interface.
 *
 * @hsotg:       The HCD state structure for the DWC OTG controller.
 * @qh:          QH for the periodic transfer.
 */
static int dwc2_uframe_schedule_hs(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh)
{
}

/**
 * dwc2_uframe_schedule_ls - Schedule a QH for a periodic low/full speed xfer.
 *
 * Basically this just wraps dwc2_ls_pmap_schedule() to provide a clean
 * interface.
 *
 * @hsotg:       The HCD state structure for the DWC OTG controller.
 * @qh:          QH for the periodic transfer.
 */
static int dwc2_uframe_schedule_ls(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh)
{
}

/**
 * dwc2_uframe_schedule - Schedule a QH for a periodic xfer.
 *
 * Calls one of the 3 sub-function depending on what type of transfer this QH
 * is for.  Also adds some printing.
 *
 * @hsotg:       The HCD state structure for the DWC OTG controller.
 * @qh:          QH for the periodic transfer.
 */
static int dwc2_uframe_schedule(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh)
{
}

/**
 * dwc2_pick_first_frame() - Choose 1st frame for qh that's already scheduled
 *
 * Takes a qh that has already been scheduled (which means we know we have the
 * bandwdith reserved for us) and set the next_active_frame and the
 * start_active_frame.
 *
 * This is expected to be called on qh's that weren't previously actively
 * running.  It just picks the next frame that we can fit into without any
 * thought about the past.
 *
 * @hsotg: The HCD state structure for the DWC OTG controller
 * @qh:    QH for a periodic endpoint
 *
 */
static void dwc2_pick_first_frame(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh)
{
	u16 frame_number;
	u16 earliest_frame;
	u16 next_active_frame;
	u16 relative_frame;
	u16 interval;

	/*
	 * Use the real frame number rather than the cached value as of the
	 * last SOF to give us a little extra slop.
	 */
	frame_number = dwc2_hcd_get_frame_number(hsotg);

	/*
	 * We wouldn't want to start any earlier than the next frame just in
	 * case the frame number ticks as we're doing this calculation.
	 *
	 * NOTE: if we could quantify how long till we actually get scheduled
	 * we might be able to avoid the "+ 1" by looking at the upper part of
	 * HFNUM (the FRREM field).  For now we'll just use the + 1 though.
	 */
	earliest_frame = dwc2_frame_num_inc(frame_number, 1);
	next_active_frame = earliest_frame;

	/* Get the "no microframe schduler" out of the way... */
	if (!hsotg->params.uframe_sched) {
		if (qh->do_split)
			/* Splits are active at microframe 0 minus 1 */
			next_active_frame |= 0x7;
		goto exit;
	}

	if (qh->dev_speed == USB_SPEED_HIGH || qh->do_split) {
		/*
		 * We're either at high speed or we're doing a split (which
		 * means we're talking high speed to a hub).  In any case
		 * the first frame should be based on when the first scheduled
		 * event is.
		 */
		WARN_ON(qh->num_hs_transfers < 1);

		relative_frame = qh->hs_transfers[0].start_schedule_us /
				 DWC2_HS_PERIODIC_US_PER_UFRAME;

		/* Adjust interval as per high speed schedule */
		interval = gcd(qh->host_interval, DWC2_HS_SCHEDULE_UFRAMES);

	} else {
		/*
		 * Low or full speed directly on dwc2.  Just about the same
		 * as high speed but on a different schedule and with slightly
		 * different adjustments.  Note that this works because when
		 * the host and device are both low speed then frames in the
		 * controller tick at low speed.
		 */
		relative_frame = qh->ls_start_schedule_slice /
				 DWC2_LS_PERIODIC_SLICES_PER_FRAME;
		interval = gcd(qh->host_interval, DWC2_LS_SCHEDULE_FRAMES);
	}

	/* Scheduler messed up if frame is past interval */
	WARN_ON(relative_frame >= interval);

	/*
	 * We know interval must divide (HFNUM_MAX_FRNUM + 1) now that we've
	 * done the gcd(), so it's safe to move to the beginning of the current
	 * interval like this.
	 *
	 * After this we might be before earliest_frame, but don't worry,
	 * we'll fix it...
	 */
	next_active_frame = (next_active_frame / interval) * interval;

	/*
	 * Actually choose to start at the frame number we've been
	 * scheduled for.
	 */
	next_active_frame = dwc2_frame_num_inc(next_active_frame,
					       relative_frame);

	/*
	 * We actually need 1 frame before since the next_active_frame is
	 * the frame number we'll be put on the ready list and we won't be on
	 * the bus until 1 frame later.
	 */
	next_active_frame = dwc2_frame_num_dec(next_active_frame, 1);

	/*
	 * By now we might actually be before the earliest_frame.  Let's move
	 * up intervals until we're not.
	 */
	while (dwc2_frame_num_gt(earliest_frame, next_active_frame))
		next_active_frame = dwc2_frame_num_inc(next_active_frame,
						       interval);

exit:
	qh->next_active_frame = next_active_frame;
	qh->start_active_frame = next_active_frame;

	dwc2_sch_vdbg(hsotg, "QH=%p First fn=%04x nxt=%04x\n",
		      qh, frame_number, qh->next_active_frame);
}

/**
 * dwc2_do_reserve() - Make a periodic reservation
 *
 * Try to allocate space in the periodic schedule.  Depending on parameters
 * this might use the microframe scheduler or the dumb scheduler.
 *
 * @hsotg: The HCD state structure for the DWC OTG controller
 * @qh:    QH for the periodic transfer.
 *
 * Returns: 0 upon success; error upon failure.
 */
static int dwc2_do_reserve(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh)
{
}

/**
 * dwc2_do_unreserve() - Actually release the periodic reservation
 *
 * This function actually releases the periodic bandwidth that was reserved
 * by the given qh.
 *
 * @hsotg: The HCD state structure for the DWC OTG controller
 * @qh:    QH for the periodic transfer.
 */
static void dwc2_do_unreserve(struct dwc2_hsotg *hsotg, struct dwc2_qh *qh)
{
	MUTEX_ASSERT_LOCKED(&hsotg->lock);

	WARN_ON(!qh->unreserve_pending);

	/* No more unreserve pending--we're doing it */
	qh->unreserve_pending = false;

	if (WARN_ON(!list_empty(&qh->qh_list_entry)))
		list_del_init(&qh->qh_list_entry);

	/* Update claimed usecs per (micro)frame */
	hsotg->periodic_usecs -= qh->host_us;

	if (hsotg->params.uframe_sched) {
		dwc2_uframe_unschedule(hsotg, qh);
	} else {
		/* Release periodic channel reservation */
		hsotg->periodic_channels--;
	}
}

/**
 * dwc2_unreserve_timer_fn() - Timer function to release periodic reservation
 *
 * According to the kernel doc for usb_submit_urb() (specifically the part about
 * "Reserved Bandwidth Transfers"), we need to keep a reservation active as
 * long as a device driver keeps submitting.  Since we're using HCD_BH to give
 * back the URB we need to give the driver a little bit of time before we
 * release the reservation.  This worker is called after the appropriate
 * delay.
 *
 * @t: Address to a qh unreserve_work.
 */
static void dwc2_unreserve_timer_fn(void *arg)
{
	struct dwc2_qh *qh = arg;
	struct dwc2_hsotg *hsotg = qh->hsotg;
	unsigned long flags;

	/*
	 * Wait for the lock, or for us to be scheduled again.  We
	 * could be scheduled again if:
	 * - We started executing but didn't get the lock yet.
	 * - A new reservation came in, but cancel didn't take effect
	 *   because we already started executing.
	 * - The timer has been kicked again.
	 * In that case cancel and wait for the next call.
	 */
	while (!spin_trylock_irqsave(&hsotg->lock, flags)) {
		if (timeout_pending(&qh->unreserve_timer))
			return;
	}

	/*
	 * Might be no more unreserve pending if:
	 * - We started executing but didn't get the lock yet.
	 * - A new reservation came in, but cancel didn't take effect
	 *   because we already started executing.
	 *
	 * We can't put this in the loop above because unreserve_pending needs
	 * to be accessed under lock, so we can only check it once we got the
	 * lock.
	 */
	if (qh->unreserve_pending)
		dwc2_do_unreserve(hsotg, qh);

	spin_unlock_irqrestore(&hsotg->lock, flags);
}
