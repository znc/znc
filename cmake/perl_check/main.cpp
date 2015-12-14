/*
 * Copyright (C) 2004-2016 ZNC, see the NOTICE file for details.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <EXTERN.h>
#include <perl.h>
#include <XSUB.h>

int perlcheck(int argc, char** argv, char** env) {
	PERL_SYS_INIT3(&argc, &argv, &env);
	PerlInterpreter* p = perl_alloc();
	perl_construct(p);
	perl_destruct(p);
	perl_free(p);
	PERL_SYS_TERM();
	return 0;
}
