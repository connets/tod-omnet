FROM ubuntu:20.04
RUN apt-get update \
    && apt-get -y install wget build-essential \
        libzmq3-dev bison flex python3 python3-pip git\  
    && rm -rf /var/lib/apt/lists/*
        #wget  g++ gdb bison flex perl \
        #libxml2-dev zlib1g-dev doxygen git \
        #python3 python3-pip \
        #tzdata libwebkit2gtk-4.0-37
        # qtbase5-dev qtchooser qt5-qmake qtbase5-dev-tools \
        #libqt5opengl5-dev graphviz \
        #libwebkit2gtk-4.0-37 mpi-default-dev \
#    && python3 -m pip install --upgrade numpy pandas matplotlib scipy seaborn posix_ipc

RUN mkdir /usr/include/nlohmann \
    && wget -O /usr/include/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp \
    && wget -O omnet.tgz https://github.com/omnetpp/omnetpp/releases/download/omnetpp-6.0/omnetpp-6.0-core.tgz \
    && tar xvfz omnet.tgz \
    && rm omnet.tgz 

WORKDIR /omnetpp-6.0
RUN bash -c "source ./setenv ; ./configure WITH_OSG=no WITH_QTENV=no PREFER_CLANG=no PREFER_LLD=no; make -j $(nproc)"


WORKDIR /app/
RUN git clone -b v4.4.1 https://github.com/inet-framework/inet.git inet4.4 \
    && git clone https://github.com/Unipisa/Simu5G Simu5G \    
    && bash -c "source /omnetpp-6.0/setenv && source inet4.4/setenv\
        && (cd inet4.4 && make makefiles && make MODE=release all -j$(nproc)) \
        && (cd Simu5G && make makefiles && make MODE=release all -j$(nproc))"
        

COPY . /app/tod_omnet_network
RUN bash -c "source /omnetpp-6.0/setenv && (cd tod_omnet_network && make makefiles && make MODE=release)"

CMD tod_omnet_network/out/gcc-release/src/tod_omnet_network -m -n tod_omnet_network/simulations:tod_omnet_network/src:inet4.4/examples:inet4.4/showcases:inet4.4/src:inet4.4/tests/validation:inet4.4/tests/networks:inet4.4/tutorials:Simu5G/emulation:Simu5G/simulations:Simu5G/src -l inet4.4/src/INET -l Simu5G/src/simu5g tod_omnet_network/simulations/small_set.ini -u Cmdenv -c Tod-Town4

#RUN bash -c "source /omnetpp-6.0/setenv"# make makefile MODE=release "
#opp_makemake -f; 
#ln -fs /usr/share/zoneinfo/Europe/Rome /etc/localtime

#g++, bison, flex, perl,  build-essential, python3 python3-pip