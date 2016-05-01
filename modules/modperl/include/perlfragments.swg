// Make perl strings to be UTF-8, they are already UTF-8 in ZNC core

%fragment("SWIG_FromCharPtrAndSize","header") {
    SWIGINTERNINLINE SV *
    SWIG_FromCharPtrAndSize(const char* carray, size_t size) {
        SV *obj = sv_newmortal();
        if (carray) {
            sv_setpvn(obj, carray, size);
        } else {
            sv_setsv(obj, &PL_sv_undef);
        }
        SvUTF8_on(obj);
        return obj;
    }
}
