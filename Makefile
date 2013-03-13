root_include_dir		:= include
root_source_dir			:= src
app_source_subdirs		:= app
lib_source_subdirs		:= lib
compile_flags    		:= -Wall -MD -pipe -fPIC -std=c++0x
link_flags			:= -pipe
libraries			:= -ldl
 
relative_include_dirs		:= $(addprefix ../../, $(root_include_dir))
app_relative_source_dirs	:= $(addprefix ../../$(root_source_dir)/, $(app_source_subdirs))
lib_relative_source_dirs	:= $(addprefix ../../$(root_source_dir)/, $(lib_source_subdirs))
app_objects_dirs		:= $(addprefix $(root_source_dir)/, $(app_source_subdirs))
lib_objects_dirs		:= $(addprefix $(root_source_dir)/, $(lib_source_subdirs))
app_objects			:= $(patsubst ../../%, %, $(wildcard $(addsuffix /*.c*, $(app_relative_source_dirs))))
lib_objects			:= $(patsubst ../../%, %, $(wildcard $(addsuffix /*.c*, $(lib_relative_source_dirs))))
app_objects			:= $(app_objects:.cpp=.o)
lib_objects			:= $(lib_objects:.cpp=.o)
 
all : $(program_name)

$(program_name): obj_dirs $(app_objects) $(lib_objects)
	g++ -shared -o ../../bin/libclient.so $(lib_objects)
	g++ -o $@ $(app_objects) $(link_flags) $(libraries) -L../../bin -lclient -lelliptics_cpp -Xlinker -rpath='./bin'
 
obj_dirs :
	mkdir -p $(app_objects_dirs)
	mkdir -p $(lib_objects_dirs)
 
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
	make --directory=./obj/Release --makefile=../../Makefile build_flags="-O2 -fomit-frame-pointer" program_name=../../bin/app

debug:
	mkdir -p bin
	mkdir -p obj
	mkdir -p obj/Debug
	make --directory=./obj/Debug --makefile=../../Makefile build_flags="-O0 -g3 -D_DEBUG" program_name=../../bin/appd
