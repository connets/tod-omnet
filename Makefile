all: checkmakefiles
	cd src && $(MAKE)

clean: checkmakefiles
	cd src && $(MAKE) clean

cleanall: checkmakefiles
	cd src && $(MAKE) MODE=release clean
	cd src && $(MAKE) MODE=debug clean
	rm -f src/Makefile

makefiles:
	cd src && opp_makemake -f --deep -O out -KCARLANETPP_PROJ=../../carlanetpp -KINET4_6_PROJ=../../inet-4.6.0 -KSIMU5G_PROJ=../../simu5g-1.4.3 -DINET_IMPORT -I$$\(CARLANETPP_PROJ\)/src -I$$\(INET4_6_PROJ\)/src -I$$\(SIMU5G_PROJ\)/src -L$$\(CARLANETPP_PROJ\)/src -L$$\(INET4_6_PROJ\)/src -L$$\(SIMU5G_PROJ\)/src -lcarlanet$$\(D\) -lINET$$\(D\) -lsimu5g$$\(D\)
	
checkmakefiles:
	@if [ ! -f src/Makefile ]; then \
	echo; \
	echo '======================================================================='; \
	echo 'src/Makefile does not exist. Please use "make makefiles" to generate it!'; \
	echo '======================================================================='; \
	echo; \
	exit 1; \
	fi
