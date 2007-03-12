# Copyright 1999-2007 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header$

inherit eutils

DESCRIPTION="znc - Advanced IRC Bouncer"
HOMEPAGE="http://znc.sourceforge.net"
SRC_URI="mirror://sourceforge/${PN}/${P}.tar.gz"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~sparc ~x86 ~x86-fbsd"
IUSE="ssl ipv6 modules debug perl sasl"

DEPEND="ssl? ( >=dev-libs/openssl-0.9.7d )
	perl? ( dev-lang/perl )
	sasl? ( dev-libs/cyrus-sasl )"
RDEPEND="${DEPEND}"

src_compile() {
	econf \
		$(use_enable ssl openssl) \
		$(use_enable ipv6) \
		$(use_enable modules) \
		$(use_enable debug) \
		$(use_enable perl) \
		$(use_enable sasl) \
		|| die "econf failed"
	emake || die "emake failed"
}

src_install() {
	make install DESTDIR="${D}" || die "make install failed."
	dohtml docs/*.html || die "dohtml failed"
	dodoc AUTHORS znc.conf || die "dodoc failed"
}

pkg_postinst() {
	einfo "You can browse the documentation here:"
	einfo "    /usr/share/doc/${PF}/*.html.gz"
	einfo "Also check out the online documentation at:"
	einfo "    http://znc.sourceforge.net"
	einfo " "
	einfo "You can find an example znc.conf file here:"
	einfo "    /usr/share/doc/${PF}/znc.conf.gz"
	einfo " "
}
