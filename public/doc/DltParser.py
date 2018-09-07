import fnmatch
import os
import re
import sys


'''
How to: 1)  Create Walker with your config file as constructor argument:
            E.g.: walker = Walker("config.txt")
        2)  Recursively search through all configured input paths and
            look for .cpp and .hpp files

Config file: The config file should look something like this:
================================================================
        # This is a comment
        # use ($THIS) to use current directory
        # multiple input paths allowed
        PATH_IN = C:/Users/max_muster/Parser/files

        # output files:
        # not all files have to be configured
        # output files can also be given with path: E.g.: FILE_ERROR = C:/Users/dlt_error.txt
        FILE_ERROR = dlt_error.txt
        FILE_WARNING = dlt_warning.txt
        FILE_INFO = dlt_info.txt
        FILE_VERBOSE = dlt_verbose.txt
================================================================
'''


class Parser:
    _line_number = 0  # current line number
    _num_brackets = 0  # number of brackets until current line
    _class_name = ""  # name of current class
    _function_name = ""  # name of current function
    _dlt_context = ""  # which category: AHD, ...
    _dlt_config = ""  # what messages will be printed: Error, Warning, ...
    _log_name = ""  # device name
    _file_in = None  # input file
    _file_out_error = None  # error file
    _file_out_warning = None  # warning file
    _file_out_info = None  # info file
    _file_out_verbose = None  # verbose file
    _lines_written = 0  # number of written lines
    _comment = ""  # current comment
    _is_commented = False  # checks if dlt messages is following a @log signature
    _debug = 1

    dlt_call = "DLT_LOG_CXX"
    dlt_error = "DLT_LOG_ERROR"
    dlt_info = "DLT_LOG_INFO"
    dlt_verbose = "DLT_LOG_VERBOSE"
    dlt_warning = "DLT_LOG_WARN"
    dlt_log = "LOG_PREFIX|LOG_DEVICE|LOG_ZONE|LOG_PORT|LOG_THREAD|LOG_BUFFER"
    msg_func = "toString|policyToString|snd_strerror|strerror|\w+->|static_cast<\w+::\w+>|\w+\."

    _patterns = []
    _regex = []

    def __init__(self, config, debug):
        print("Initialising parser: ...")
        self._debug = debug
        print("\tDebug mode = " + str(self._debug))
        self._dlt_config = ""
        is_set = 0
        if config.get("Error"):
            print("\tPrinting error messages.")
            self._dlt_config += self.dlt_error
            is_set = 1
        if config.get("Info"):
            print("\tPrinting info messages.")
            if is_set:
                self._dlt_config += '|'
            else:
                is_set = 1
            self._dlt_config += self.dlt_info
        if config.get("Verbose"):
            print("\tPrinting verbose messages.")
            if is_set:
                self._dlt_config += '|'
            else:
                is_set = 1
            self._dlt_config += self.dlt_verbose
        if config.get("Warning"):
            print("\tPrinting warning messages.")
            if is_set:
                self._dlt_config += '|'
            self._dlt_config += self.dlt_warning
        print("\tSetting patterns.")
        self.set_patterns()
        print("\tCompiling patterns: ...")
        self.compile_patterns(True)
        print("Done!\n")

    def set_patterns(self):
        self._patterns = ["(?:DLT_REGISTER_CONTEXT\(\*mLog,\s*\"|getDltContext\(\")(\w+)(\")",  # DLT context
                          "(?:cClassName\s*=\s*\")(\w+)(?:::\";)",  # class name
                          "(?:cClassName::)(\~?\w+)\((\s*\w+|\))",  # function name
                          "(?:\s*|namespace\s+\w*\s*){",  # open bracket
                          "(?:\s*)}",  # close bracket
                          "(?:#define\s+)(" + self.dlt_log + ")(?:.*)",  # log defines
                          "(/\*\*)",  # comment start
                          self.dlt_call + "\(\*\w+,\s*(" + self._dlt_config + ")"]  # DLT message

    def compile_patterns(self, display=False):
        self._regex = []
        for pattern in self._patterns:
            if self._debug and display:
                print("\t\t" + pattern)
            self._regex.append(re.compile(pattern))

    def open_input_file(self, in_file):
        if self._debug:
            print("Opening input file: ...")
        if self._file_in is not None:
            if self._debug:
                print("\tFile already opened! Please close the file first.")
            return
        if self._debug:
            print("\t" + in_file)
        try:
            self._file_in = open(in_file, "r")
        except OSError:
            if self._debug:
                print("\tFailed!\n")
        else:
            if self._debug:
                print("\tSucceeded!\n")
        self._class_name = ""
        self._function_name = ""
        self._log_name = ""
        self._dlt_context = ""
        self._line_number = 0
        self._num_brackets = 0
        self._lines_written = 0
        self._is_commented = False
        self.set_patterns()
        self.compile_patterns()

    def close_input_file(self):
        if self._debug:
            print("Closing input file: ...")
        if self._file_in is None:
            if self._debug:
                print("\tFile already closed!")
            return
        try:
            self._file_in.close()
        except OSError:
            if self._debug:
                print("\tFile non-existent!\n")
        else:
            if self._debug:
                print("\tSucceeded! Lines written = " + str(self._lines_written) + ".\n")
            self._file_in = None

    def open_output_files(self, files):
        if self._debug:
            print("Opening output files: ...")
        for out_file, path in files.items():
            if out_file == "Error":
                if self._file_out_error is not None:
                    if self._debug:
                        print("\tError file already opened! Please close the file first.")
                    return
                if self._debug:
                    print("\t" + path)
                try:
                    self._file_out_error = open(path, "w+")
                    self._file_out_error.write("<table class=\"doxtable\">\n")
                    self._file_out_error.write("<tr><th>Context ID <th>DLT Message <th>Description\n")
                except OSError:
                    if self._debug:
                        print("\tFailed!")
                else:
                    if self._debug:
                        print("\tSucceeded!")
            if out_file == "Warning":
                if self._file_out_warning is not None:
                    if self._debug:
                        print("\tWarning file already opened! Please close the file first.")
                    return
                if self._debug:
                    print("\t" + path)
                try:
                    self._file_out_warning = open(path, "w+")
                    self._file_out_warning.write("<table class=\"doxtable\">\n")
                    self._file_out_warning.write("<tr><th>Context ID <th>DLT Message <th>Description\n")
                except OSError:
                    if self._debug:
                        print("\tFailed!")
                else:
                    if self._debug:
                        print("\tSucceeded!")
            if out_file == "Info":
                if self._file_out_info is not None:
                    if self._debug:
                        print("\tInfo file already opened! Please close the file first.")
                    return
                if self._debug:
                    print("\t" + path)
                try:
                    self._file_out_info = open(path, "w+")
                    self._file_out_info.write("<table class=\"doxtable\">\n")
                    self._file_out_info.write("<tr><th>Context ID <th>DLT Message <th>Description\n")
                except OSError:
                    if self._debug:
                        print("\tFailed!")
                else:
                    if self._debug:
                        print("\tSucceeded!")
            if out_file == "Verbose":
                if self._file_out_verbose is not None:
                    if self._debug:
                        print("\tVerbose file already opened! Please close the file first.")
                    return
                if self._debug:
                    print("\t" + path)
                try:
                    self._file_out_verbose = open(path, "w+")
                    self._file_out_verbose.write("<table class=\"doxtable\">\n")
                    self._file_out_verbose.write("<tr><th>Context ID <th>DLT Message <th>Description\n")
                except OSError:
                    if self._debug:
                        print("\tFailed!\n")
                else:
                    if self._debug:
                        print("\tSucceeded!\n")

    def close_output_files(self):
        if self._debug:
            print("Closing output files: ...")
        if self._file_out_error is None:
            if self._debug:
                print("\tError file already closed!")
        else:
            try:
                self._file_out_error.write("</table>\n")
                self._file_out_error.close()
            except OSError:
                if self._debug:
                    print("\tFile non-existent!")
            else:
                if self._debug:
                    print("\tSucceeded!")
                self._file_out_error = None
        if self._file_out_warning is None:
            if self._debug:
                print("\tWarning file already closed!")
        else:
            try:
                self._file_out_warning.write("</table>\n")
                self._file_out_warning.close()
            except OSError:
                if self._debug:
                    print("\tFile non-existent!")
            else:
                if self._debug:
                    print("\tSucceeded!")
                self._file_out_warning = None
        if self._file_out_info is None:
            if self._debug:
                print("\tInfo file already closed!")
        else:
            try:
                self._file_out_info.write("</table>\n")
                self._file_out_info.close()
            except OSError:
                if self._debug:
                    print("\tFile non-existent!")
            else:
                if self._debug:
                    print("\tSucceeded!")
                self._file_out_info = None
        if self._file_out_verbose is None:
            if self._debug:
                print("\tError file already closed!")
        else:
            try:
                self._file_out_verbose.write("</table>\n")
                self._file_out_verbose.close()
            except OSError:
                if self._debug:
                    print("\tFile non-existent!\n")
            else:
                if self._debug:
                    print("\tSucceeded!\n")
                self._file_out_verbose = None

    def scan_file(self):
        if self._debug:
            print("Scanning file: ...")
        if self._file_in is None:
            if self._debug:
                print("\tFailed! Make sure to open a file first.\n")
            return
        else:
            if self._debug:
                print("\t" + self._file_in.name)
        while True:
            line = self._file_in.readline()
            if not line:
                break
            self._line_number += 1
            expr_idx = 0
            for expression in self._regex:
                if expression.search(line):
                    if expr_idx == 0:  # DLT context
                        self._dlt_context = re.split(expression, line)[1]

                    elif expr_idx == 1:  # class name
                        self._class_name = re.split(expression, line)[1]
                        # replace cClassName in function name pattern
                        new_pattern = re.sub("cClassName", self._class_name, self._regex[2].pattern)
                        self._regex[2] = re.compile(new_pattern)

                    elif expr_idx == 2:  # function name
                        if self._class_name:
                            self._function_name = re.split(expression, line)[1]
                        else:
                            if self._debug:
                                print("Failed! Class name not found yet.")
                            return

                    elif expr_idx == 3:  # open bracket
                        # print("\tOpen bracket: " + line)
                        self._num_brackets += 1

                    elif expr_idx == 4:  # close bracket
                        # print("\tClose bracket: " + line)
                        self._num_brackets -= 1

                    elif expr_idx == 5:  # log defines
                        define = re.split(expression, line)[1]
                        if define == "LOG_DEVICE":
                            self._log_name += "device=&lt;NAME&gt;: "
                        elif define == "LOG_ZONE":
                            self._log_name += "zone=&lt;NAME&gt;: "
                        elif define == "LOG_PORT":
                            self._log_name += "port=&lt;NAME&gt;: "
                        elif define == "LOG_THREAD":
                            self._log_name += "thread=&lt;NAME&gt;: "

                    elif expr_idx == 6:  # comment start
                        new_line = self._file_in.readline()
                        self._line_number += 1
                        if re.search("\*\s*@log", new_line):
                            self._is_commented = True
                            self.parse_comment(new_line)
                        # write DLT message
                        line = self._file_in.readline()
                        self._line_number += 1
                        if re.search(self._regex[7], line):
                            if self._is_commented:
                                self.write_dlt_message(line)
                                self._lines_written += 1
                        self._is_commented = False
                expr_idx += 1
        if self._num_brackets > 0:
            if self._debug:
                print("\tFailed! Number of brackets > 0.\n")
        else:
            if self._debug:
                print("\tDone!\n")

    def write_dlt_message(self, line):
        if self._debug:
            if not self._class_name:
                print("\tFailure! No class name.")
            if not self._function_name:
                print("\tFailure! No function name.")
            if not self._dlt_context:
                print("\tFailure! No DLT context.")

        out_file = ""
        if re.search(self.dlt_error, line):
            out_file = "Error"
        elif re.search(self.dlt_warning, line):
            out_file = "Warning"
        elif re.search(self.dlt_info, line):
            out_file = "Info"
        elif re.search(self.dlt_verbose, line):
            out_file = "Verbose"
        else:
            print("Failure! No DLT file identifier.")

        # set up message header
        message = "<tr><td>" + self._dlt_context + "<td>" + self._class_name + "::" + self._function_name + "(" + \
            str(self._line_number) + "): "

        # add log name specifier
        if self._log_name:
            message += self._log_name

        # parse error message
        err_msg = self.parse_error_message(line)
        # format and add error message
        err_msg = self.format_message(err_msg)
        message += err_msg

        # add comment
        message = message + "<td>" + self._comment + "\n"

        # write the message
        if self._debug:
            print("\t" + message)
        if out_file == "Error":
            self._file_out_error.write(message)
        elif out_file == "Warning":
            self._file_out_warning.write(message)
        elif out_file == "Info":
            self._file_out_info.write(message)
        elif out_file == "Verbose":
            self._file_out_verbose.write(message)

    def format_message(self, message):
        variables = re.split("\".+?\"", message)
        for idx, variable in enumerate(variables):
            if not re.search("\w+", variable):
                variables[idx] = ""
        variables = list(filter(bool, variables))

        strings = re.findall("\".+?\"", message)
        strings = list(filter(len, strings))

        for idx, string in enumerate(strings):
            strings[idx] = re.sub("\"", "", string)

        for idx, variable in enumerate(variables):
            for count in range(2):
                variable = re.sub(self.msg_func + "|,|\(|\)| |\n", "", variable)
                variables[idx] = variable.upper()

        formatted = ""
        while True:
            try:
                formatted += strings.pop(0)
                formatted = formatted + " &lt;" + variables.pop(0) + "&gt; "
            except IndexError:
                break

        return formatted

    def parse_comment(self, line):
        self._comment = ""
        self._comment += re.split("(?:@log\s+)(.*)", line)[1]
        line = self._file_in.readline()
        self._line_number += 1
        while not re.search("\*/", line):
            self._comment += re.split("(?:\s*\*\s*)(.*)", line)[1]
            line = self._file_in.readline()
            self._line_number += 1
        self._comment = re.sub("<", "&lt;", self._comment)
        self._comment = re.sub(">", "&gt;", self._comment)

    def parse_error_message(self, line):
        err_msg = ""
        # all in one line
        if re.search("\);\s*$", line):
            err_msg = re.split("(?:" + self.dlt_log + ")(?:\s*,\s*\")(.*)(?:\);\s*$)", line)
            if len(err_msg) > 1:
                err_msg = "\"" + err_msg[1]
            return err_msg
        # nothing in this line
        elif re.search(self.dlt_log + "\s*,\s*$", line):
            line = self._file_in.readline()
            self._line_number += 1
            while not re.search("\);\s*$", line):
                err_msg += line
                line = self._file_in.readline()
                self._line_number += 1
            last_msg = re.split("(.*)(?:\);\s*$)", line)
            if len(last_msg) > 1:
                err_msg += last_msg[1]
        # something in this line
        elif re.search(self.dlt_log + "\s*,\s*\"", line):
            err_msg = re.split("(?:" + self.dlt_log + "\s*,\s\"*)(.*)", line)
            if len(err_msg) > 1:
                err_msg = "\"" + err_msg[1]
            line = self._file_in.readline()
            self._line_number += 1
            while not re.search("\);\s*$", line):
                err_msg += line
                line = self._file_in.readline()
                self._line_number += 1
            last_msg = re.split("(.*)(?:\);\s*$)", line)
            if len(last_msg) > 1:
                err_msg += last_msg[1]

        return err_msg


class Walker:
    _config_file = None
    _config_file_name = ""
    _input_root = ""
    _debug = 1
    _parser = Parser

    _path_in = []
    _out_files = {}

    _extensions = [".cpp", ".hpp"]

    def __init__(self, config_file, input_root):
        print("Initialising file walker: ...")
        try:
            self._config_file = open(config_file)
        except OSError:
            print("\tFailure! Configuration file not found.")
            return
        self._input_root = input_root
        self._config_file_name = config_file
        self.load_config()
        self._parser = Parser(self._out_files, self._debug)
        print("Done!\n")

    def load_config(self):
        print("\tLoading config file: ...")
        if not len(self._config_file_name):
            print("\t\tFailure! No file opened.")
            return
        print("\t\t" + self._config_file_name)
        # read lines of config file
        for line in self._config_file:
            # ignore comments
            if re.search("(#\s*.*)", line):
                continue
            # debug
            elif re.search("DEBUG", line):
                values = re.split("(?:DEBUG\s*=\s*)(\d)", line)
                if len(values) > 1:
                    self._debug = int(values[1])
            # input path
            elif re.search("PATH_IN", line):
                values = re.split("(?:PATH_IN\s*=\s*)(\S+)", line)
                if values[1] == "($THIS)":
                    self._path_in.append(self._input_root + "")
                else:
                    self._path_in.append(self._input_root + "/" + values[1])
                print("\t\tInput path = " + self._path_in[-1])
            # output file path\name
            elif re.search("FILE_", line):
                values = re.split("(?:FILE_)(\w+)(?:\s*=\s*)([\w\./]+)", line)
                if len(values) > 1:
                    if values[1] == "ERROR":
                        self._out_files["Error"] = os.path.dirname(os.path.realpath(__file__)) + "/" + values[2]
                    elif values[1] == "WARNING":
                        self._out_files["Warning"] = os.path.dirname(os.path.realpath(__file__)) + "/" + values[2]
                    elif values[1] == "INFO":
                        self._out_files["Info"] = os.path.dirname(os.path.realpath(__file__)) + "/" + values[2]
                    elif values[1] == "VERBOSE":
                        self._out_files["Verbose"] = os.path.dirname(os.path.realpath(__file__)) + "/" + values[2]
        print("\tSucceeded!\n")

    def search_paths(self):
        self._parser.open_output_files(self._out_files)
        for path in self._path_in:
            self.search_recursively(path)
        self._parser.close_output_files()

    def search_recursively(self, path):
        matches = []
        for extension in self._extensions:
            for root, dir_names, file_names in os.walk(path, onerror=printerr):
                for filename in fnmatch.filter(file_names, '*' + extension):
                    matches.append(os.path.join(root, filename))
                    self._parser.open_input_file(root + "/" + filename)
                    self._parser.scan_file()
                    self._parser.close_input_file()


def printerr(err):
    print(err)

script_path = os.path.dirname(os.path.realpath(__file__))
if len(sys.argv) > 1:
    in_root = sys.argv[1]
else:
    in_root = script_path + "/../../../.."

cfg_file = "config.txt"
walker = Walker(script_path + "/" + cfg_file, in_root)
walker.search_paths()
