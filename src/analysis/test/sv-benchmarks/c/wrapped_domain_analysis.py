#!/usr/bin/python

import sys, getopt
import os
from os import environ, path
import subprocess
from subprocess import Popen
import logging
import collections
import json
import time

def print_all_opts():
  print('wrapped_domain_analysis.py -i <ifile> -f <examplefile> -m <max_disjunctions> -d <debug_level> -o <use_oct> -r <use_red_prod> -e <use_extrapolation> -n <perform_narrowing> -a <array_bounds_check> -p <allow_phis> -w <disable_wrapping>')

def main(argv):
   inputfilestr = ''
   max_disjunctions = '1'
   examplefilestr = ''
   debug_level = '0'
   use_oct = False
   use_red_prod = False
   use_extrapolation = False
   perform_narrowing = False
   array_bounds_check = False
   allow_phis = False
   disable_wrapping = False
   try:
      opts, args = getopt.getopt(argv,"hi:f:m:d:oenarpw",["help","ifile=","examplefile=","max_disjunctions=","debug_level=", "use_oct", "use_extrapolation", "perform_narrowing", "array_bounds_check", "use_red_prod", "allow_phis", "disable_wrapping"])
   except getopt.GetoptError:
      print_all_opts()
      sys.exit(2)
   for opt, arg in opts:
      if opt == '-h':
         print_all_opts()
         sys.exit()
      elif opt in ("-i", "--ifile"):
         inputfilestr = arg
      elif opt in ("-f", "--examplefile"):
         examplefilestr = arg
      elif opt in ("-m", "--max_disjunctions"):
         max_disjunctions = arg
      elif opt in ("-d", "--debug_level"):
         debug_level = arg
      elif opt in ("-o", "--use_oct"):
         use_oct = True
      elif opt in ("-r", "--use_red_prod"):
         use_red_prod = True
      elif opt in ("-e", "--use_extrapolation"):
         use_extrapolation = True
      elif opt in ("-n", "--perform_narrowing"):
         perform_narrowing = True
      elif opt in ("-a", "--array_bounds_check"):
         array_bounds_check = True
      elif opt in ("-p", "--allow_phis"):
         allow_phis = True
      elif opt in ("-w", "--disable_wrapping"):
         disable_wrapping = True

   print('Input file is ', inputfilestr)
   print('Example file is ', examplefilestr)
   print('Maximum allowed disjunctions is ', max_disjunctions)
   print('Debug level is ', debug_level)
   print('Use oct ', use_oct)
   print('Use extrapolation ', use_extrapolation)
   print('Perform narrowing ', perform_narrowing)
   print('Allow phis ', allow_phis)

   # If the user supplies an example file using f, ignore the input file given by i, and just run the
   # analysis on single file f, by creating a tmp file to store its address
   if examplefilestr != '':
      inputfilestr = ".tmp.wrapped_domain_analysis"
      inputfile = open(inputfilestr, 'w')
      inputfile.write(examplefilestr)
      inputfile.close()


   env = dict(os.environ)
   env['LD_LIBRARY_PATH'] = os.path.abspath("../../../../../external/lib") + ";" + os.path.abspath("../../../../../lib")
   print('Using LD_LIBRARY_PATH ', env['LD_LIBRARY_PATH'])
   inputfile = open(inputfilestr)

   # Store the result of running each example in result 
   num_total_examples = 0
   num_timedout_examples = 0
   num_crashed_examples = 0
   num_total_assertions = 0
   num_total_instr = 0

   results = [] 
   for __line in inputfile:
     timedout = False
     _line = __line.rstrip()
     line = os.path.abspath(_line)
     line_woext = os.path.splitext(line)[0]
     print('\n\n------------Processing input', line_woext, '------------------------------\n')
     line_bc = line_woext + '.bc'
     line_log = line_woext + '.log'
     logfile = open(line_log, 'w')
     print('line_bc is ', line_bc)
     args_cl = "../../../../../external/bin/clang -c -O0 -emit-llvm " + line + " -o " + line_bc
     try:
       print("Executing " + args_cl)
       p_cl = subprocess.Popen(args_cl, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
       p_cl_out, p_cl_err = p_cl.communicate()
       print(p_cl_out)
       num_total_examples += 1
     except (OSError, subprocess.CalledProcessError) as exception:
       print('Subprocess failed. Cannot create bc file using clang on ' + line)
       print('Exception occured: ' + str(exception))
       continue

     if array_bounds_check:
       args_opt = "../../../../../external/bin/opt -bounds-checking -stats " + line_bc + " -o " + line_bc
       try:
         print("Executing " + args_opt)
         p_opt = subprocess.Popen(args_opt, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
         p_opt_out, p_opt_err = p_opt.communicate()
         print(p_opt_out)
       except (OSError, subprocess.CalledProcessError) as exception:
         print('Subprocess failed. Cannot perform bounds-checking pass on bc file with opt on ' + line)
         print('Exception occured: ' + str(exception))
         continue

     sys.stdout.flush()
     start = time.time()
     args = "../../../../../bin/bvsfdAnalysis --filename " + line_bc + " --max_disjunctions " + max_disjunctions + " --debug_print_level " + debug_level
     if array_bounds_check:
       args = args + " --array_bounds_check"
     if use_oct:
       args = args + " --use_oct"
     if use_extrapolation:
       args = args + " --use_extrapolation"
     if perform_narrowing:
       args = args + " --perform_narrowing"
     if allow_phis:
       args = args + " --allow_phis"
     if disable_wrapping:
       args = args + " --disable_wrapping"

     try:
       print('Executing ', args)
       sys.stdout.flush()
       p = subprocess.Popen(args, stdout=logfile, stderr=subprocess.STDOUT, env=env, shell=True)
       p_out, p_err = p.communicate(timeout = 200)
       print(p_out)
     except subprocess.TimeoutExpired as exception:
       print(exception)
       num_timedout_examples += 1
       timedout = True
     except (OSError, subprocess.CalledProcessError) as exception:
       print('Subprocess failed. Cannot perform wrappeddomain analysis on ' + line)
       print('Exception occured: ' + str(exception))
       continue

     sys.stdout.flush()
     timetaken = time.time() - start

     # Store the result of the analysis into results
     try:
       line_result = line_bc + '.result'
       resultfile = open(line_result)
       result_input_stats = resultfile.readline()
       result_output_stats = resultfile.readline()
       print('Input stats to the analysis', result_input_stats)
       print('Output stats to the analysis', result_output_stats)
       results.append((line_bc, result_input_stats, result_output_stats, timetaken, timedout))
       resultfile.close()
     except Exception as exception:
       print ("Warning: This shouldn't be happening. Readline should not give exceptions.")
       if not timedout:
         num_crashed_examples += 1
       print(exception)

     sys.stdout.flush()
     # For loop ends here

   inputfile.close()
   print ("max_disjunctions is ", max_disjunctions)
   print ("use_oct is ", use_oct)
   print ("use_extrapolation is ", use_extrapolation)
   print ("perform_narrowing is ", perform_narrowing)
   print ("Allow phis is ", allow_phis)
   print ("Disable wrapping is ", disable_wrapping)
   print ("debug_level is ", debug_level)

   # Print results in a manner that it can be picked up by a tex file
   print('\n---------------------------------------------------------------------------------------\n')
   if len(results) != 0:
     # Preserve original order of result so that C++ result printer can manipulate results
     decoder = json.JSONDecoder(object_pairs_hook=collections.OrderedDict)

     # Find a result such that the decoder doesn't crash on it, which happens when the example itself
     # crashed.
     foundnoncrashresult = False
     input_key_list = []
     output_key_list = []
     for (name, result_input_stats, result_output_stats, timetaken, timedout) in results:
       # Print the labels of the dictionary and store them in a list
       try:
         result_input_odict = decoder.decode(result_input_stats)
         for key, val in result_input_odict.items():
           # Ignore keys on Abstraction and num disjs, just print them once
           if key == "name" or key == "Abstraction" or key == "Num Disjs" or key == "Use extrapolation" or key == "Perform narrowing":
             print(key, " is ", val)
             continue
           input_key_list.append(key)

         result_output_odict = decoder.decode(result_output_stats)
         for key, val in result_output_odict.items():
           output_key_list.append(key)

         print("name & ", ' & '.join(str(x) for x in input_key_list), " & ", ' & '.join(str(x) for x in output_key_list), " & total time", " \\\\")

         foundnoncrashresult = True
         break
       except ValueError as err:
         continue


     num_total_time = 0.0
     max_example_time = 0.0
     num_total_proved_assertions = 0
     num_total_arrayboundscheck_assertions = 0
     num_total_proved_arrayboundscheck_assertions = 0
     min_voc_size = 100000 # A very high number
     max_voc_size = 0
     for (name, result_input_stats, result_output_stats, timetaken, is_timedout) in results:
       is_crashed = False
       name = os.path.relpath(name)
       name = name.replace("_", "\_")
       name = "$" + name + "$"
       result_str_line = ""
       try:
         result_input_odict = decoder.decode(result_input_stats)
         val_input_list = []
         for key, val in result_input_odict.items():
           val_input_list.append(val)
         result_str_line = name + ' & '.join(str(x) for x in val_input_list) + ' & '
       except ValueError as err:
         is_crashed = True
         result_str_line = name + len(input_key_list)* " & ? " +  " & "

       try:
         result_output_odict = decoder.decode(result_output_stats)
         val_output_list = []
         for key, val in result_output_odict.items():
           val_output_list.append(val)
         result_str_line = result_str_line + ' & '.join(str(x) for x in val_output_list) + " & " + str(timetaken) + " \\\\"
       except ValueError as err:
         is_crashed = True
         result_str_line = result_str_line + len(output_key_list)* " crash & " + str(timetaken) + "  \\\\"

       print (result_str_line)
       sys.stdout.flush()

       if is_crashed and not is_timedout:
         num_crashed_examples += 1
       else:
         num_total_instr += int(result_input_odict["Num Instrs"])
         num_total_assertions += int(result_input_odict["Num Assertions"])
         num_total_proved_assertions += int(result_output_odict["Num Proved Assertions"])
         if array_bounds_check:
           num_total_arrayboundscheck_assertions += int(result_input_odict["Num Array Bounds Check Assertions"])
           num_total_proved_arrayboundscheck_assertions += int(result_output_odict["Num Proved Array Bounds Check Assertions"])
         num_total_time += float(timetaken)
         if max_example_time < float(timetaken):
           max_example_time = float(timetaken)
         crt_min_voc_size = int(result_input_odict["Min voc size"])
         crt_max_voc_size = int(result_input_odict["Max voc size"])
         if min_voc_size > crt_min_voc_size:
           min_voc_size = crt_min_voc_size
         if max_voc_size < crt_max_voc_size:
           max_voc_size = crt_max_voc_size

   print('Input file is ', inputfilestr)
   print('Example file is ', examplefilestr)
   print('Maximum allowed disjunctions is ', max_disjunctions)
   print('Debug level is ', debug_level)
   print('Use oct ', use_oct)
   print('Use extrapolation ', use_extrapolation)
   print('Perform narrowing ', perform_narrowing)
   print('Allow phis ', allow_phis)

   if array_bounds_check:
     print('Benchmark & num_total_examples & num_crashed_examples & num_timedout_examples & num_total_instr & voc_range & num_total_assertions & num_total_proved_assertions & num_total_arrayboundscheck_assertions & num_total_proved_arrayboundscheck_assertions & num_total_time \\\\')
     print(inputfilestr, " & ", str(num_total_examples), " & ", str(num_crashed_examples), " & ", str(num_timedout_examples), " & ", str(num_total_instr), " & ", str(min_voc_size)+"-"+str(max_voc_size), " & ", str(num_total_assertions), " & ", str(num_total_proved_assertions), " & ", str(num_total_arrayboundscheck_assertions), " & ", str(num_total_proved_arrayboundscheck_assertions), " & ", str(num_total_time), " \\\\")
   else:
     print('Benchmark & num_total_examples & num_crashed_examples & num_timedout_examples & num_total_instr & voc_range & num_total_assertions & num_total_proved_assertions & num_total_time \\\\')
     print(inputfilestr, " & ", str(num_total_examples), " & ", str(num_crashed_examples), " & ", str(num_timedout_examples), " & ", str(num_total_instr), " & ", str(min_voc_size)+"-"+str(max_voc_size), " & ", str(num_total_assertions), " & ", str(num_total_proved_assertions), " & ", str(num_total_time), " \\\\")
     
   print('max_example_time,', max_example_time)
   print('Printing space separated line for excel file.')
   print(max_disjunctions, " ", str(num_total_proved_assertions), " ", str(num_total_time))
   if array_bounds_check:
     print(max_disjunctions, " ", str(num_total_proved_arrayboundscheck_assertions), " ", str(num_total_time))
   
if __name__ == "__main__":
   main(sys.argv[1:])
