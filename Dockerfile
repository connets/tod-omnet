FROM ubuntu:20.04
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive TZ=Etc/UTC \ 
    apt-get -y install \
        wget build-essential bison flex python3 python3-pip git\  
        #wget  g++ gdb bison flex perl \
        #libxml2-dev zlib1g-dev doxygen git \
        #python3 python3-pip \
        #tzdata libwebkit2gtk-4.0-37
        # qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools \
        #libqt5opengl5-dev graphviz \
        #libwebkit2gtk-4.0-37 mpi-default-dev \
    && rm -rf /var/lib/apt/lists/*
#    && python3 -m pip install --upgrade numpy pandas matplotlib scipy seaborn posix_ipc

RUN wget -O omnet.tgz https://github.com/omnetpp/omnetpp/releases/download/omnetpp-6.0/omnetpp-6.0-core.tgz \
    && tar xvfz omnet.tgz \
    && rm omnet.tgz 

WORKDIR /omnetpp-6.0
RUN bash -c "source ./setenv ; ./configure WITH_OSG=no WITH_QTENV=no PREFER_CLANG=no PREFER_LLD=no; make -j $(nproc)"
WORKDIR /

COPY . /tod_network/carla_omnet_network
WORKDIR /tod_network/
RUN git clone -b v4.4.1 https://github.com/inet-framework/inet.git inet4.4 \
    && (cd inet4.4 && make makefiles && make all -j$(nproc)) \
    && git clone https://github.com/Unipisa/Simu5G simu5G \
    && (cd simu5G && make all -j$(nproc)) \
    && (cd /tod_network/carla_omnet_network && make makefiles && make MODE=release)

#RUN bash -c "source /omnetpp-6.0/setenv"# make makefile MODE=release "
#opp_makemake -f; 
#ln -fs /usr/share/zoneinfo/Europe/Rome /etc/localtime

#g++, bison, flex, perl,  build-essential, python3 python3-pip