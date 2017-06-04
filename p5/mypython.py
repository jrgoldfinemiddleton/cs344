# CS 344 - Oregon State University
# Author: Jason Goldfine-Middleton
# Date: 10 July 2016
# Using Python 2.6

import random

def create_file(suffix):
    '''Creates a file with 10 lowercase characters and a newline.
    
    The output file has prefix 'outfile' and the given suffix.  The
    contents of the created file are printed.  If a file of the same
    name already exists, it is overwritten.
    '''

    # open file safely and then close automatically after block
    with open('outfile' + suffix, 'w') as f:
        # create lowercase string, write to file, then print it
        rstr = gen_lowercase_str(10) + '\n'
        f.write(rstr)
        print 'contents of file ' + f.name + ': ' + rstr
    return

def gen_lowercase_str(length):
    '''Returns a random string of lowercase characters.
    '''
    word = ''
    # create as many characters as requested
    for i in range(length):
        # use ASCII values of 'a' and 'z' to set range
        ascii_val = random.randint(ord('a'), ord('z'))
        # append the resulting char from the random ASCII value
        word += str(unichr(ascii_val))
    return word

def prnt_rand_product(min_val, max_val):
    '''Generates two random integers in given range and prints their product.
    '''
    val1 = random.randint(min_val, max_val)
    val2 = random.randint(min_val, max_val)
    print str(val1) + ' * ' + str(val2) + ' = ' + str(val1 * val2)
    return

random.seed()
create_file('0')
create_file('1')
create_file('2')
prnt_rand_product(1, 42)
