#!/bin/bash
export PATH=$PATH:/unsup/python3/amd64_rhel6/bin
python3 wrapped_domain_analysis.py -i all-examples.txt -m 1 -w -d 0 -p  2>&1 | tee all-examples-poly-1-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 2 -w -d 0 -p  2>&1 | tee all-examples-poly-2-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 3 -w -d 0 -p  2>&1 | tee all-examples-poly-3-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 4 -w -d 0 -p  2>&1 | tee all-examples-poly-4-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 5 -w -d 0 -p  2>&1 | tee all-examples-poly-5-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 6 -w -d 0 -p  2>&1 | tee all-examples-poly-6-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 7 -w -d 0 -p  2>&1 | tee all-examples-poly-7-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 8 -w -d 0 -p  2>&1 | tee all-examples-poly-8-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 9 -w -d 0 -p  2>&1 | tee all-examples-poly-9-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 10 -w -d 0 -p  2>&1 | tee all-examples-poly-10-nowrapping.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 1 -d 0 -p  2>&1 | tee all-examples-poly-1.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 2 -d 0 -p  2>&1 | tee all-examples-poly-2.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 3 -d 0 -p  2>&1 | tee all-examples-poly-3.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 4 -d 0 -p  2>&1 | tee all-examples-poly-4.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 5 -d 0 -p  2>&1 | tee all-examples-poly-5.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 6 -d 0 -p  2>&1 | tee all-examples-poly-6.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 7 -d 0 -p  2>&1 | tee all-examples-poly-7.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 8 -d 0 -p  2>&1 | tee all-examples-poly-8.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 9 -d 0 -p  2>&1 | tee all-examples-poly-9.out
python3 wrapped_domain_analysis.py -i all-examples.txt -m 10 -d 0 -p  2>&1 | tee all-examples-poly-10.out
