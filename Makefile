GCCPATH = D:\dev\mingw
APPNAME = Console
RESFILE = resources
PARSER = parser
SCANNER = scanner

ifndef DEBUG
	CFLAGS =   -Os
  LDFLAGS = -shared -s -fno-exceptions -fno-rtti
else
	CFLAGS = -Wall
	LDFLAGS = -shared
endif

$(APPNAME).dll : $(APPNAME).o $(RESFILE).o 
	$(GCCPATH)\bin\gcc $(LDFLAGS) -o $(APPNAME).dll $(APPNAME).o $(RESFILE).o  -lgdi32

$(APPNAME).o : $(APPNAME).c
	$(GCCPATH)\bin\gcc $(CFLAGS) -c $(APPNAME).c

$(RESFILE).o : $(RESFILE).rc
	$(GCCPATH)\bin\windres -o $(RESFILE).o $(RESFILE).rc
		
