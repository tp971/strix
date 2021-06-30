JAVA_HOME=$(shell dirname $$(dirname $$(readlink -f $$(which javac))))
BIN_DIR=bin
BUILD_DIR=build
BIN_NAME=strix
OWL_VERSION=owl-minimized-19.XX-development
ZIP_NAME=distributions/$(OWL_VERSION).zip
JAR_NAME_FULL=owl-19.XX-development-all.jar
JAR_NAME=owl.jar

all: $(BIN_DIR)/$(JAR_NAME) $(BIN_DIR)/$(BIN_NAME)

clean:
	rm -rf $(BUILD_DIR)

clean-owl:
	rm -f $(BIN_DIR)/$(JAR_NAME) $(BUILD_DIR)/$(ZIP_NAME) $(BUILD_DIR)/libs/$(JAR_NAME_FULL)

distclean: clean
	rm -rf $(BIN_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR)/$(JAR_NAME): $(BUILD_DIR)/$(ZIP_NAME) | $(BIN_DIR)
	unzip -u -j -o -d $(BIN_DIR) $< $(OWL_VERSION)/lib/$(JAR_NAME_FULL)
	mv $(BIN_DIR)/$(JAR_NAME_FULL) $@
	touch $@

$(BUILD_DIR)/$(ZIP_NAME): lib/owl | $(BUILD_DIR)
	GRADLE_OPTS=-Dorg.gradle.project.buildDir=../../$(BUILD_DIR) lib/owl/gradlew minimizedDistZip -plib/owl --project-cache-dir=../../$(BUILD_DIR)/.gradle

$(BUILD_DIR)/Makefile: $(OWL_JAR) | $(BUILD_DIR)
	(cd $(BUILD_DIR) && cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=Release -DJAVA_HOME=$(JAVA_HOME) ..)

$(BIN_DIR)/$(BIN_NAME): $(BUILD_DIR)/Makefile $(BIN_DIR)/$(JAR_NAME)
	@ $(MAKE) -C $(BUILD_DIR) --no-print-directory

test: $(BIN_DIR)/$(BIN_NAME) $(BUILD_DIR)/Makefile $(BIN_DIR)/$(JAR_NAME)
	(cd $(BUILD_DIR) && ctest)

.PHONY: all clean clean-owl distclean test $(BIN_DIR)/$(BIN_NAME)
