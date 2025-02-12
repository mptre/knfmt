/*
 * Qualifier regression.
 */

Symbol_Type *symbols_type_insert_pointer(Symbols *,
    const Symbol_Type_Identifier, const Symbol_Type_Identifier);

void foo(const volatile Foo, int);
