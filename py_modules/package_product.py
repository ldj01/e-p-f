
""" Description: Create tarball and checksum files
"""

import os
import logging
import glob
import datetime
from collections import OrderedDict
from espa_constants import SUCCESS, ERROR
from espa import System

# Global variables
logger = logging.getLogger(__name__)

def create_tarball(source_directory, product_name):
    """ Creates the tarball and checksum file.

        Parameters:
            source_directory: Directory where files to be tar'ed up are
            located.
            product_name: name of tarball to be created.

        Returns:
            tarball_filename
            checksum_filename
    """

    # construct the tarball name
    tarball_filename = '%s.tar.gz' % product_name

    # construct the md5sum filename
    checksum_filename = '%s_MD5.txt' % product_name

    # change to the source directory
    current_directory = os.getcwd()
    os.chdir(source_directory)

    # get files to tar and gzip
    file_list = glob.glob("*")

    tar_cmd = 'tar -cz '

    tar_cmd += ' '.join(file_list)

    check_pipe_status = '; test ${PIPESTATUS[0]} -eq 0 -a ${PIPESTATUS[1]}' \
        ' -eq 0 -a ${PIPESTATUS[2]} -eq 0'

    cmd = '%s | tee %s/%s | md5sum -> %s/%s %s' % (tar_cmd,
        current_directory, tarball_filename, current_directory,
        checksum_filename, check_pipe_status)

    output = ''
    try:
        output = System.execute_cmd(cmd)
    except Exception:
        msg = "Error encountered tarring files: Stdout/Stderr:"
        if len(output) > 0:
            msg = ' '.join([msg, output])
        else:
            msg = ' '.join([msg, 'NO STDOUT/STDERR'])
        logger.error(msg)
        raise Exception(msg)

    os.chdir(current_directory)

    read_and_fix_checksum_file(checksum_filename, tarball_filename)

    return (tarball_filename, checksum_filename)

def update_product_data_file(source_dir, tarball_filename,
    checksum_filename):
    """ Updates the product_data.txt file with tarball and checksum file info

        Parameters:
            source_dir: directory where initial product_data.txt file resides
            tarball_filename: Name of tarball
            checksum_filename: Name of checksum file
    """

    # get size and date of tarball
    tarball_st = os.stat(tarball_filename)
    tarball_size = tarball_st.st_size

    # get date for tarball
    tarball_datetime = datetime.datetime.fromtimestamp(tarball_st.st_mtime)
    tarball_date = "%s/%s/%s %s:%s:%s" % (tarball_datetime.year,
        tarball_datetime.month, tarball_datetime.day,
        tarball_datetime.hour, tarball_datetime.minute,
        tarball_datetime.second)
     
    # get size and date of checksum file
    checksum_st = os.stat(checksum_filename)
    checksum_size = checksum_st.st_size

    # get date for checksum file
    checksum_datetime = datetime.datetime.fromtimestamp(checksum_st.st_mtime)
    checksum_date = "%s/%s/%s %s:%s:%s" % (checksum_datetime.year,
        checksum_datetime.month, checksum_datetime.day,
        checksum_datetime.hour, checksum_datetime.minute,
        checksum_datetime.second)
    
    # append tarball name to product_data.txt

    product_data_filename = 'product_data.txt'

    # read product_data.txt into dictionary
    product_data = OrderedDict({})
    with open(product_data_filename) as myfile:
        for line in myfile:
            name, var = line.partition("=")[::2]
            product_data[name.strip()] = var.strip()

    product_data["L2_PACKAGE_NAME"] = tarball_filename

    product_data["L2_PACKAGE_FILE_SIZE"] = tarball_size

    product_data["L2_PACKAGE_FILE_DATE"] = tarball_date

    product_data["L2_CHECKSUM_FILE_NAM"] = checksum_filename

    product_data["L2_CHECKSUM_FILE_SIZE"] = checksum_size

    product_data["L2_CHECKSUM_FILE_DATE"] = checksum_date

    output_filename = "product_data.tmp"

    with open(output_filename, 'w') as outfile:
        for name, var in product_data.items():
            outfile.write("%s=%s\n" % (name, var))

    os.rename(output_filename, product_data_filename)

def read_and_fix_checksum_file(checksum_filename, tarball_filename):
    """ Replace '-' in checksum file with tarball name

        Parameters:
            checksum_filename: Name of checksum file
            tarball_filename: name of tarball
    """

    output_filename = "checksum_file.tmp"

    with open(checksum_filename, 'r') as infile:
        in_line = infile.read()
   
    for line in in_line:
        checksum, filename = in_line.split()

    with open(output_filename, 'w') as outfile:
        outfile.write("%s  %s\n" % (checksum, tarball_filename))

    os.rename(output_filename, checksum_filename)

def package_product(source_directory, product_name):
    """ Creates tarball for Level 2 products

        Parameters:
            source_directory: directory containing files included in package
            product_name: Name of product to be packaged

        Returns: SUCCESS, ERROR
    """

    try:
        (tarball_filename, checksum_filename) = create_tarball(source_directory,
            product_name)

        update_product_data_file(source_directory, tarball_filename,
            checksum_filename)
    except Exception as ex:
        logger.error(str(ex))
        logger.error('The product package was not created properly')
        return ERROR

    logger.info('Product Package processing complete.')

    return SUCCESS
