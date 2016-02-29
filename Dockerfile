FROM centos:latest

# Add daemon
ADD build/src/openelpd /usr/local/bin/openelpd

# Add library
ADD build/src/libopenelp.so.0.6.0 /usr/local/lib/libopenelp.so.0.6.0

# set up symlinks
RUN ln -s /usr/local/lib/libopenelp.so.0.6.0 /usr/local/lib/libopenelp.so.0
RUN ln -s /usr/local/lib/libopenelp.so.0 /usr/local/lib/libopenelp.so

# Add library path to ldconf for Centos
RUN echo /usr/local/lib > /etc/ld.so.conf.d/usrlocal.conf

# Copy in default conf
ADD doc/ELProxy.conf /etc/ELProxy.conf.default

# Update cache to find libopenlp.so.*
RUN /sbin/ldconfig

# Set default command to run OpenELP
CMD sed "s/notset/${password-notset}/g" /etc/ELProxy.conf.default  > /etc/ELProxy.conf ; \
    /usr/local/bin/openelpd -F
