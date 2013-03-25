root_include_dir		:= include
root_source_dir			:= src
app_source_subdirs		:= app
lib_source_subdirs		:= lib
fastcgi_source_subdirs	:= fastcgi
compile_flags			:= -Wall -MD -pipe -fPIC -std=c++0x
link_flags				:= -pipe
libraries				:= -ldl
 
relative_include_dirs		:= $(addprefix ../../, $(root_include_dir))

app_relative_source_dirs	:= $(addprefix ../../$(root_source_dir)/, $(app_source_subdirs))
app_objects_dirs			:= $(addprefix $(root_source_dir)/, $(app_source_subdirs))
app_objects					:= $(patsubst ../../%, %, $(wildcard $(addsuffix /*.cpp, $(app_relative_source_dirs))))
app_objects					:= $(app_objects:.cpp=.o)

lib_relative_source_dirs	:= $(addprefix ../../$(root_source_dir)/, $(lib_source_subdirs))
lib_objects_dirs			:= $(addprefix $(root_source_dir)/, $(lib_source_subdirs))
lib_objects					:= $(patsubst ../../%, %, $(wildcard $(addsuffix /*.cpp, $(lib_relative_source_dirs))))
lib_objects					:= $(lib_objects:.cpp=.o)

fastcgi_relative_source_dirs	:= $(addprefix ../../$(root_source_dir)/, $(fastcgi_source_subdirs))
fastcgi_objects_dirs			:= $(addprefix $(root_source_dir)/, $(fastcgi_source_subdirs))
fastcgi_objects					:= $(patsubst ../../%, %, $(wildcard $(addsuffix /*.cpp, $(fastcgi_relative_source_dirs))))
fastcgi_objects					:= $(fastcgi_objects:.cpp=.o)

all : $(program_name)

$(program_name): obj_dirs $(app_objects) $(lib_objects) $(fastcgi_objects)
	g++ -shared -o ../../bin/libclient.so $(lib_objects) -lboost_thread
	g++ -shared -o ../../bin/libhistorydb-fastcgi.so $(fastcgi_objects) -lfastcgi-daemon2 -pthread -L../../bin -lclient -lelliptics_cpp -lboost_thread -Xlinker -rpath='./bin'
	g++ -o $@ $(app_objects) $(link_flags) $(libraries) -L../../bin -lclient -lelliptics_cpp -lboost_thread -Xlinker -rpath='./bin'
 
obj_dirs :
	mkdir -p $(app_objects_dirs)
	mkdir -p $(lib_objects_dirs)
	mkdir -p $(fastcgi_objects_dirs)
 
VPATH := ../../
 
%.o : %.cpp
	g++ -o $@ -c $< $(compile_flags) $(build_flags) $(addprefix -I, $(relative_include_dirs))
 
.PHONY : clean
 
clean :
	rm -rf bin obj
 
include $(wildcard $(addsuffix /*.d, $(objects_dirs)))

release:
	mkdir -p bin
	mkdir -p obj
	mkdir -p obj/Release
	make --directory=./obj/Release --makefile=../../Makefile build_flags="-O1 -fomit-frame-pointer" program_name=../../bin/app

debug:
	mkdir -p bin
	mkdir -p obj
	mkdir -p obj/Debug
	make --directory=./obj/Debug --makefile=../../Makefile build_flags="-O0 -g3 -D_DEBUG" program_name=../../bin/appd
