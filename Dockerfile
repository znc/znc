FROM alpine:3.3

RUN adduser -S znc && addgroup -S znc

RUN mkdir -p /home/znc/src /home/znc/build
WORKDIR /home/znc/build
ADD . /home/znc/src

ARG CONFIGUREFLAGS="--enable-perl --enable-python 3.5"
ARG CLEANCMD="apk del build-dependencies && rm -Rf /home/znc/build /home/znc/src"
ARG MAKEFLAGS=""

RUN apk add --no-cache --virtual runtime-dependencies \
        icu \
        openssl \
        boost \
        python3 \
        perl \
        cyrus-sasl

RUN apk add --no-cache --virtual build-dependencies \
        build-base \
        cmake \
        git \
        icu-dev \
        openssl-dev \
        cyrus-sasl-dev \
        perl-dev \
        python3-dev \
        swig \
        gettext-dev \
        boost-dev \
    && /home/znc/src/configure.sh $CONFIGUREFLAGS \
    && make $MAKEFLAGS \
    && make install \
    && sh -c "$CLEANCMD"

USER znc
WORKDIR /home/znc
VOLUME /home/znc/data

EXPOSE 6667

ENTRYPOINT ["/usr/local/bin/znc", "-f", "-d", "/home/znc/data"]
