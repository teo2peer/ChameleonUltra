FROM --platform=linux/amd64 ubuntu:22.04@sha256:0d779ea97881505f5ef0039336ee85edba27519bdba968c284c86ee066a973c8

ARG ARM_TOOLCHAIN_VERSION=12.2.rel1
ARG NRF_CLT_URL=https://nsscprodmedia.blob.core.windows.net/prod/software-and-other-downloads/desktop-software/nrf-command-line-tools/sw/versions-10-x-x/10-22-1/nrf-command-line-tools_10.22.1_amd64.deb
ARG NRFUTIL_SHA256=4800f8b71feec0caaf3c143d29b0b7fd6f33c6a258da10e660743f0db1890a0b
ARG NRF_CLT_SHA256=00a317f2fa6c4e6935e1de9b8a32c2b258cb949a8d0f2b221a5165752df3857c
ARG ARM_TOOLCHAIN_SHA256=84be93d0f9e96a15addd490b6e237f588c641c8afdf90e7610a628007fc96867

RUN set -xe; \
  DEBIAN_FRONTEND=noninteractive; \
  apt update -q; \
  apt install -qy --no-install-recommends ca-certificates curl xz-utils make git zip; \
  git config --global --add safe.directory /workdir; \
  curl -fsSLo /usr/bin/nrfutil https://developer.nordicsemi.com/.pc-tools/nrfutil/x64-linux/nrfutil; \
  echo "$NRFUTIL_SHA256  /usr/bin/nrfutil" | sha256sum -c -; \
  curl -fsSLo /tmp/nrf-clt.deb "$NRF_CLT_URL"; \
  echo "$NRF_CLT_SHA256  /tmp/nrf-clt.deb" | sha256sum -c -; \
  apt install -qfy /tmp/nrf-clt.deb; \
  rm /tmp/nrf-clt.deb; \
  chmod 755 /usr/bin/nrfutil; \
  nrfutil install device nrf5sdk-tools; \
  curl -fsSLo /tmp/arm-toolchain.tar.xz https://armkeil.blob.core.windows.net/developer/Files/downloads/gnu/$ARM_TOOLCHAIN_VERSION/binrel/arm-gnu-toolchain-$ARM_TOOLCHAIN_VERSION-x86_64-arm-none-eabi.tar.xz; \
  echo "$ARM_TOOLCHAIN_SHA256  /tmp/arm-toolchain.tar.xz" | sha256sum -c -; \
  tar xJf /tmp/arm-toolchain.tar.xz -C /usr/local; \
  rm /tmp/arm-toolchain.tar.xz; \
  apt clean -q; \
  rm -rf /var/lib/apt/lists/*


ENV GNU_INSTALL_ROOT=/usr/local/arm-gnu-toolchain-$ARM_TOOLCHAIN_VERSION-x86_64-arm-none-eabi/bin/
ENV GNU_VERSION=$ARM_TOOLCHAIN_VERSION

WORKDIR /workdir
