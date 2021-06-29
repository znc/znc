FROM alpine:3.12

ARG VERSION_EXTRA=""

ARG CMAKEFLAGS="-DVERSION_EXTRA=${VERSION_EXTRA} -DCMAKE_INSTALL_PREFIX=/opt/znc -DWANT_CYRUS=YES -DWANT_PERL=YES -DWANT_PYTHON=YES"
ARG MAKEFLAGS=""

LABEL org.label-schema.schema-version="1.0"
LABEL org.label-schema.vcs-url="https://github.com/znc/znc"
LABEL org.label-schema.url="https://znc.in"

COPY . /znc-src

RUN set -x \
    && adduser -S znc \
    && addgroup -S znc
RUN apk add --no-cache \
        boost \
        build-base \
        ca-certificates \
        cmake \
        cyrus-sasl \
        gettext \
        icu-dev \
        libressl-dev \
        perl \
        python3 \
        su-exec \
        tini \
        tzdata
RUN apk add --no-cache --virtual build-dependencies \
        boost-dev \
        cyrus-sasl-dev \
        perl-dev \
        python3-dev \
        swig \
    && cd /znc-src \
    && mkdir build && cd build \
    && cmake .. ${CMAKEFLAGS} \
    && make $MAKEFLAGS \
    && make install \
    && apk del build-dependencies \
    && cd / && rm -rf /znc-src

COPY docker/slim/entrypoint.sh /
COPY docker/*/??-*.sh /startup-sequence/

VOLUME /znc-data

ENTRYPOINT ["/entrypoint.sh"]
