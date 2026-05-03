PKGNAME=	sysrc-1.1
PKGREVISION=	2
CATEGORIES=	mate
MASTER_SITES=
DISTNAME=

MAINTAINER=	nervoso@k1.com.br
HOMEPAGE=	http://www.k1.com.br/
COMMENT=	edit rc.conf as FreeBSD does

WRKSRC=		${WRKDIR}/${PKGNAME}

USE_LANGUAGES+=	c
USE_TOOLS+=	pkg-config

INSTALLATION_DIRS+=share/examples/sysrc 
INSTALLATION_DIRS+=share/examples/rc.d
INSTALLATION_DIRS+=bin

do-extract:
	mkdir -p ${WRKSRC}
	cp -rpf ${FILESDIR}/* ${WRKSRC}

.include "../../devel/glib2/buildlink3.mk"
.include "../../mk/bsd.pkg.mk"
