FROM ubuntu:20.04
RUN apt-get update \
    && apt-get -y  --no-install-recommends install wget build-essential \
        libzmq3-dev bison flex python3 python3-pip git\  
    && rm -rf /var/lib/apt/lists/*

RUN mkdir /usr/include/nlohmann \
    && wget -O /usr/include/nlohmann/json.hpp https://github.com/nlohmann/json/releases/download/v3.11.2/json.hpp \
    && wget -O omnet.tgz https://github.com/omnetpp/omnetpp/releases/download/omnetpp-6.0/omnetpp-6.0-core.tgz \
    && tar xvfz omnet.tgz \
    && rm omnet.tgz 

WORKDIR /omnetpp-6.0
RUN bash -c "source ./setenv ; ./configure WITH_OSG=no WITH_QTENV=no PREFER_CLANG=no PREFER_LLD=no; make -j $(nproc)"


WORKDIR /app/
RUN git clone https://github.com/Jaivra/inet inet4.4 \
    && git clone https://github.com/Jaivra/Simu5G Simu5G \
    && bash -c "source /omnetpp-6.0/setenv && source inet4.4/setenv \
        && (cd inet4.4 && make makefiles && make MODE=release all -j$(nproc)) \
        && (cd Simu5G && make makefiles && make MODE=release all -j$(nproc))"

RUN git clone https://github.com/Jaivra/carlanetpp \ 
    && bash -c "source /omnetpp-6.0/setenv \
        && (cd carlanetpp && make makefiles && make MODE=release all -j$(nproc))"
        

COPY . /app/tod-omnet
RUN bash -c "source /omnetpp-6.0/setenv && (cd tod-omnet && make makefiles && make MODE=release)"

CMD tod-omnet/out/gcc-release/src/tod-omnet -m -n tod-omnet/simulations:tod-omnet/src:inet4.4/examples:inet4.4/showcases:inet4.4/src:inet4.4/tests/validation:inet4.4/tests/networks:inet4.4/tutorials:Simu5G/emulation:Simu5G/simulations:Simu5G/src:carlanetpp/src -l carlanetpp/src/carlanet -l inet4.4/src/INET -l Simu5G/src/simu5g tod-omnet/simulations/vnc.ini -u Cmdenv --cmdenv-stop-batch-on-error=false --cmdenv-express-mode=true -c Tod-Town4-With-Background --result-dir=../results
