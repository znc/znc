# Copyright 1999-2005 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header$

inherit eutils

DESCRIPTION="znc - Advanced IRC Bouncer"
HOMEPAGE="http://sourceforge.net/projects/znc"
SRC_URI="mirror://sourceforge/${PN}/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="alpha amd64 arm ppc sparc x86"
IUSE="nossl nomodules debug"

RDEPEND="virtual/libc"
DEPEND="virtual/libc
		!nossl? ( dev-libs/openssl )"

S="${WORKDIR}/${P}"

src_unpack() {
	unpack ${A} || die "unpack failed"
}

src_compile() {
	local MY_CONFARGS=""

	use nossl && MY_CONFARGS="${MY_CONFARGS} --disable-openssl"
	use nomodules && MY_CONFARGS="${MY_CONFARGS} --disable-modules"
	use debug && MY_CONFARGS="${MY_CONFARGS} --enable-debug"

	econf ${MY_CONFARGS} || die "econf failed"
	emake CFLAGS="${CFLAGS}" || die "emake failed"
}

src_install() {
	make install DESTDIR=${D}
	dodoc znc.conf
}

pkg_postinst() {
	einfo "You can find an example znc.conf file here:"
	einfo " /usr/share/doc/${PF}/znc.conf.gz"
}
