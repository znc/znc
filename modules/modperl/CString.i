/*
   SWIG-generated sources are used here.
   */


%{
#include <string>
%}




%feature("naturalvar") CString;
class CString;


/*@SWIG:/usr/share/swig1.3/typemaps/std_strings.swg,74,%typemaps_std_string@*/

/*@SWIG:/usr/share/swig1.3/typemaps/std_strings.swg,4,%std_string_asptr@*/
%fragment("SWIG_" "AsPtr" "_" {CString},"header",fragment="SWIG_AsCharPtrAndSize") {
SWIGINTERN int
SWIG_AsPtr_std_string SWIG_PERL_DECL_ARGS_2(SV * obj, CString **val)
{
    char* buf = 0 ; size_t size = 0;
    int found = 0;
    if (SvMAGICAL(obj)) {
        SV *tmp = sv_newmortal();
        SvSetSV(tmp, obj);
        obj = tmp;
    }
    static int init = 0;
    static swig_type_info* descriptor = 0;
    if (!init) {
        descriptor = SWIG_TypeQuery("CString" " *");
        init = 1;
    }
    if (descriptor) {
        CString *vptr;
        int res = SWIG_ConvertPtr(obj, (void**)&vptr, descriptor, 0);
        if (SWIG_IsOK(res)) {
            if (val) *val = vptr;
            return res;
        }
    }
    swig_type_info* pchar_descriptor = SWIG_pchar_descriptor();
    if (pchar_descriptor) {
        char* vptr = 0;
        if (SWIG_ConvertPtr(obj, (void**)&vptr, pchar_descriptor, 0) == SWIG_OK) {
            buf = vptr;
            size = vptr ? (strlen(vptr) + 1) : 0;
            found = 1;
        }
    }
    if (!found) {
        STRLEN len = 0;
        buf = SvPV(obj, len);
        size = len + 1;
        found = 1;
    }
    if (found) {
        if (buf) {
            if (val) *val = new CString(buf, size - 1);
            return SWIG_NEWOBJ;
        } else {
            if (val) *val = 0;
            return SWIG_OLDOBJ;
        }
    }
    return SWIG_ERROR;
}
}
/*@SWIG@*/
/*@SWIG:/usr/share/swig1.3/typemaps/std_strings.swg,52,%std_string_asval@*/
%fragment("SWIG_" "AsVal" "_" {CString},"header", fragment="SWIG_" "AsPtr" "_" {CString}) {
SWIGINTERN int
SWIG_AsVal_std_string SWIG_PERL_DECL_ARGS_2(SV * obj, CString *val)
{
  CString* v = (CString *) 0;
  int res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2(obj, &v);
  if (!SWIG_IsOK(res)) return res;
  if (v) {
	if (val) *val = *v;
	if (SWIG_IsNewObj(res)) {
	  free((char*)v);
	  res = SWIG_DelNewMask(res);
	}
	return res;
  }
  return SWIG_ERROR;
}
}
/*@SWIG@*/
/*@SWIG:/usr/share/swig1.3/typemaps/std_strings.swg,38,%std_string_from@*/
%fragment("SWIG_" "From" "_" {CString},"header",fragment="SWIG_FromCharPtrAndSize") {
SWIGINTERNINLINE SV *
SWIG_From_std_string  SWIG_PERL_DECL_ARGS_1(const CString& s)
{
  if (s.size()) {
	return SWIG_FromCharPtrAndSize(s.data(), s.size());
  } else {
	return SWIG_FromCharPtrAndSize(s.c_str(), 0);
  }
}
}
/*@SWIG@*/

/*@SWIG:/usr/share/swig1.3/typemaps/ptrtypes.swg,201,%typemaps_asptrfromn@*/
/*@SWIG:/usr/share/swig1.3/typemaps/ptrtypes.swg,190,%typemaps_asptrfrom@*/
  /*@SWIG:/usr/share/swig1.3/typemaps/ptrtypes.swg,160,%typemaps_asptr@*/
  %fragment("SWIG_" "AsVal" "_" {CString},"header",fragment="SWIG_" "AsPtr" "_" {CString}) {
	SWIGINTERNINLINE int
	SWIG_AsVal_std_string SWIG_PERL_CALL_ARGS_2(SV * obj, CString *val)
	{
	  CString *v = (CString *)0;
	  int res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2(obj, &v);
	  if (!SWIG_IsOK(res)) return res;
	  if (v) {
	if (val) *val = *v;
	if (SWIG_IsNewObj(res)) {
	  free((char*)v);
	  res = SWIG_DelNewMask(res);
	}
	return res;
	  }
	  return SWIG_ERROR;
	}
  }
  /*@SWIG:/usr/share/swig1.3/typemaps/ptrtypes.swg,28,%ptr_in_typemap@*/
  %typemap(in,fragment="SWIG_" "AsPtr" "_" {CString}) CString {
	CString *ptr = (CString *)0;
	int res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2($input, &ptr);
	if (!SWIG_IsOK(res) || !ptr) {
	  SWIG_exception_fail(SWIG_ArgError((ptr ? res : SWIG_TypeError)), "in method '" "$symname" "', argument " "$argnum"" of type '" "$type""'");
	}
	$1 = *ptr;
	if (SWIG_IsNewObj(res)) free((char*)ptr);
  }
  %typemap(freearg) CString "";
  %typemap(in,fragment="SWIG_" "AsPtr" "_" {CString}) const CString & (int res = SWIG_OLDOBJ) {
	CString *ptr = (CString *)0;
	res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2($input, &ptr);
	if (!SWIG_IsOK(res)) { SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument " "$argnum"" of type '" "$type""'"); }
	if (!ptr) { SWIG_exception_fail(SWIG_ValueError, "invalid null reference " "in method '" "$symname" "', argument " "$argnum"" of type '" "$type""'"); }
	$1 = ptr;
  }
  %typemap(freearg,noblock=1) const CString &  {
	if (SWIG_IsNewObj(res$argnum)) free((char*)$1);
  }
/*@SWIG@*/;
  /*@SWIG:/usr/share/swig1.3/typemaps/ptrtypes.swg,53,%ptr_varin_typemap@*/
  %typemap(varin,fragment="SWIG_" "AsPtr" "_" {CString}) CString {
	CString *ptr = (CString *)0;
	int res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2($input, &ptr);
	if (!SWIG_IsOK(res) || !ptr) {
	  SWIG_exception_fail(SWIG_ArgError((ptr ? res : SWIG_TypeError)), "in variable '""$name""' of type '""$type""'");
	}
	$1 = *ptr;
	if (SWIG_IsNewObj(res)) free((char*)ptr);
  }
/*@SWIG@*/;
  ;
  /*@SWIG:/usr/share/swig1.3/typemaps/ptrtypes.swg,143,%ptr_typecheck_typemap@*/
%typemap(typecheck,noblock=1,precedence=135,fragment="SWIG_" "AsPtr" "_" {CString}) CString * {
  int res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2($input, (CString**)(0));
  $1 = SWIG_CheckState(res);
}

%typemap(typecheck,noblock=1,precedence=135,fragment="SWIG_" "AsPtr" "_" {CString}) CString, const CString& {
  int res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2($input, (CString**)(0));
  $1 = SWIG_CheckState(res);
}
/*@SWIG@*/;
  /*@SWIG:/usr/share/swig1.3/typemaps/inoutlist.swg,254,%ptr_input_typemap@*/
  /*@SWIG:/usr/share/swig1.3/typemaps/inoutlist.swg,117,%_ptr_input_typemap@*/
  %typemap(in,noblock=1,fragment="SWIG_" "AsPtr" "_" {CString}) CString *INPUT(int res = 0) {
	res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2($input, &$1);
	if (!SWIG_IsOK(res)) {
	  SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument " "$argnum"" of type '" "$type""'");
	}
	res = SWIG_AddTmpMask(res);
  }
  %typemap(in,noblock=1,fragment="SWIG_" "AsPtr" "_" {CString}) CString &INPUT(int res = 0) {
	res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2($input, &$1);
	if (!SWIG_IsOK(res)) {
	  SWIG_exception_fail(SWIG_ArgError(res), "in method '" "$symname" "', argument " "$argnum"" of type '" "$type""'");
	}
	if (!$1) {
	  SWIG_exception_fail(SWIG_ValueError, "invalid null reference " "in method '" "$symname" "', argument " "$argnum"" of type '" "$type""'");
	}
	res = SWIG_AddTmpMask(res);
  }
  %typemap(freearg,noblock=1,match="in") CString *INPUT, CString &INPUT {
	if (SWIG_IsNewObj(res$argnum)) free((char*)$1);
  }
  %typemap(typecheck,noblock=1,precedence=135,fragment="SWIG_" "AsPtr" "_" {CString}) CString *INPUT, CString &INPUT {
	int res = SWIG_AsPtr_std_string SWIG_PERL_CALL_ARGS_2($input, (CString**)0);
	$1 = SWIG_CheckState(res);
  }
/*@SWIG@*/
/*@SWIG@*/;
/*@SWIG@*/
  /*@SWIG:/usr/share/swig1.3/typemaps/valtypes.swg,184,%typemaps_from@*/
  /*@SWIG:/usr/share/swig1.3/typemaps/valtypes.swg,55,%value_out_typemap@*/
  %typemap(out,noblock=1,fragment="SWIG_" "From" "_" {CString}) CString, const CString {
	 $result = SWIG_From_std_string  SWIG_PERL_CALL_ARGS_1((CString)($1)); argvi++ ;
  }
  %typemap(out,noblock=1,fragment="SWIG_" "From" "_" {CString}) const CString& {
	 $result = SWIG_From_std_string  SWIG_PERL_CALL_ARGS_1((CString)(*$1)); argvi++ ;
  }
/*@SWIG@*/;
  /*@SWIG:/usr/share/swig1.3/typemaps/valtypes.swg,79,%value_varout_typemap@*/
  %typemap(varout,noblock=1,fragment="SWIG_" "From" "_" {CString}) CString, const CString&  {
	 sv_setsv($result,SWIG_From_std_string  SWIG_PERL_CALL_ARGS_1((CString)($1)))  ;
  }
/*@SWIG@*/;
  /*@SWIG:/usr/share/swig1.3/typemaps/valtypes.swg,87,%value_constcode_typemap@*/
  %typemap(constcode,noblock=1,fragment="SWIG_" "From" "_" {CString}) CString {
	/*@SWIG:/usr/share/swig1.3/perl5/perltypemaps.swg,65,%set_constant@*/ do {
  SV *sv = get_sv((char*) SWIG_prefix "$symname", TRUE | 0x2 | GV_ADDMULTI);
  sv_setsv(sv, SWIG_From_std_string  SWIG_PERL_CALL_ARGS_1((CString)($value)));
  SvREADONLY_on(sv);
} while(0) /*@SWIG@*/;
  }
/*@SWIG@*/;
  ;
  /*@SWIG:/usr/share/swig1.3/typemaps/valtypes.swg,154,%value_throws_typemap@*/
  %typemap(throws,noblock=1,fragment="SWIG_" "From" "_" {CString}) CString {
	 sv_setsv(GvSV(PL_errgv), SWIG_From_std_string  SWIG_PERL_CALL_ARGS_1((CString)($1))); SWIG_fail ;
  }
/*@SWIG@*/;
  /*@SWIG:/usr/share/swig1.3/typemaps/inoutlist.swg,258,%value_output_typemap@*/
  /*@SWIG:/usr/share/swig1.3/typemaps/inoutlist.swg,175,%_value_output_typemap@*/
 %typemap(in,numinputs=0,noblock=1)
   CString *OUTPUT ($*1_ltype temp, int res = SWIG_TMPOBJ),
   CString &OUTPUT ($*1_ltype temp, int res = SWIG_TMPOBJ) {
   $1 = &temp;
 }
 %typemap(argout,noblock=1,fragment="SWIG_" "From" "_" {CString}) CString *OUTPUT, CString &OUTPUT {
   if (SWIG_IsTmpObj(res$argnum)) {
	  if (argvi >= items) EXTEND(sp,1);  $result = SWIG_From_std_string  SWIG_PERL_CALL_ARGS_1((*$1)); argvi++  ;
   } else {
	 int new_flags = SWIG_IsNewObj(res$argnum) ? (SWIG_POINTER_OWN | $shadow) : $shadow;
	  if (argvi >= items) EXTEND(sp,1);  $result = SWIG_NewPointerObj((void*)($1), $1_descriptor, new_flags); argvi++  ;
   }
 }
/*@SWIG@*/
/*@SWIG@*/;
/*@SWIG@*/;
  /*@SWIG:/usr/share/swig1.3/typemaps/inoutlist.swg,258,%value_output_typemap@*/
  /*@SWIG:/usr/share/swig1.3/typemaps/inoutlist.swg,175,%_value_output_typemap@*/
 %typemap(in,numinputs=0,noblock=1)
   CString *OUTPUT ($*1_ltype temp, int res = SWIG_TMPOBJ),
   CString &OUTPUT ($*1_ltype temp, int res = SWIG_TMPOBJ) {
   $1 = &temp;
 }
 %typemap(argout,noblock=1,fragment="SWIG_" "From" "_" {CString}) CString *OUTPUT, CString &OUTPUT {
   if (SWIG_IsTmpObj(res$argnum)) {
	  if (argvi >= items) EXTEND(sp,1);  $result = SWIG_From_std_string  SWIG_PERL_CALL_ARGS_1((*$1)); argvi++  ;
   } else {
	 int new_flags = SWIG_IsNewObj(res$argnum) ? (SWIG_POINTER_OWN | $shadow) : $shadow;
	  if (argvi >= items) EXTEND(sp,1);  $result = SWIG_NewPointerObj((void*)($1), $1_descriptor, new_flags); argvi++  ;
   }
 }
/*@SWIG@*/
/*@SWIG@*/;
  /*@SWIG:/usr/share/swig1.3/typemaps/inoutlist.swg,240,%_ptr_inout_typemap@*/
 /*@SWIG:/usr/share/swig1.3/typemaps/inoutlist.swg,230,%_value_inout_typemap@*/
 %typemap(in) CString *INOUT = CString *INPUT;
 %typemap(in) CString &INOUT = CString &INPUT;
 %typemap(typecheck) CString *INOUT = CString *INPUT;
 %typemap(typecheck) CString &INOUT = CString &INPUT;
 %typemap(argout) CString *INOUT = CString *OUTPUT;
 %typemap(argout) CString &INOUT = CString &OUTPUT;
/*@SWIG@*/
 %typemap(typecheck) CString *INOUT = CString *INPUT;
 %typemap(typecheck) CString &INOUT = CString &INPUT;
 %typemap(freearg) CString *INOUT = CString *INPUT;
 %typemap(freearg) CString &INOUT = CString &INPUT;
/*@SWIG@*/;
/*@SWIG@*/;
/*@SWIG@*/;

/*@SWIG@*/;


