all: 
	@if [ "`uname -s`" = "Linux" ] ; then \
		make -f Makefile.linux ; \
	else \
		make -f Makefile.sunos ; \
	fi

clean:
	@rm -rf myftpclient myftpserver
	@rm -rf *.o
