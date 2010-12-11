GCCPATH = C:\Progra~1\MinGW
APPNAME = Console
RESFILE = resources
PARSER = parser
SCANNER = scanner

ifndef DEBUG
	CFLAGS = -Os -Wall
	LDFLAGS = -shared -s -fno-exceptions -fno-rtti
else
	CFLAGS = -Wall
	LDFLAGS = -shared
endif

$(APPNAME).dll : $(APPNAME).o $(RESFILE).o $(PARSER).o $(SCANNER).o
	$(GCCPATH)\bin\gcc $(LDFLAGS) -o $(APPNAME).dll $(APPNAME).o $(RESFILE).o $(PARSER).o $(SCANNER).o -lgdi32

$(APPNAME).o : $(APPNAME).c
	$(GCCPATH)\bin\gcc $(CFLAGS) -c $(APPNAME).c

$(RESFILE).o : $(RESFILE).rc
	$(GCCPATH)\bin\windres -o $(RESFILE).o $(RESFILE).rc
	
$(PARSER).o : $(PARSER).c
	$(GCCPATH)\bin\gcc -Os -c $(PARSER).c

$(PARSER).c $(PARSER).h : $(PARSER).y
	bison.exe --defines --output=$(PARSER).c $(PARSER).y    

$(SCANNER).o : $(SCANNER).c
	$(GCCPATH)\bin\gcc -Os -c $(SCANNER).c	

$(SCANNER).c : $(SCANNER).l $(PARSER).h
	flex.exe -o$(SCANNER).c $(SCANNER).l
	