
""" Description: Create tarball and checksum files
"""

import os
import hashlib
import logging
import glob
import datetime as dt
from collections import OrderedDict
import subprocess


# Global variables
logger = logging.getLogger(__name__)


def create_tarball(source_directory, product_id):
    """ Creates the tarball and checksum file.

        Parameters:
            source_directory: Directory where files to be tar'ed up are
                              located.
            product_id: Level 2 product Id

        Returns:
            tarball_filename
            checksum_filename
    """

    # Construct the tarball name
    tarball_filename = '%s.tar.gz' % product_id

    # Construct the md5sum filename
    checksum_filename = '%s_MD5.txt' % product_id

    # Change to the source directory
    current_directory = os.getcwd()
    os.chdir(source_directory)

    # Get files to include in the package
    file_list = glob.glob("*")

    # Tar and gzip files
    tar_cmd = 'tar -cz ' + ' '.join(file_list)

    check_pipe_status = '; test ${PIPESTATUS[0]} -eq 0 -a ${PIPESTATUS[1]}' \
        ' -eq 0 -a ${PIPESTATUS[2]} -eq 0'

    cmd = '%s | tee %s/%s | md5sum -> %s/%s %s' % (
        tar_cmd, current_directory, tarball_filename, current_directory,
        checksum_filename, check_pipe_status)

    try:
        subprocess.check_output(cmd, shell=True)
    except subprocess.CalledProcessError as e:
        raise

    os.chdir(current_directory)

    read_and_fix_checksum_file(checksum_filename, tarball_filename)

    return (tarball_filename, checksum_filename)


def update_product_data_file(source_dir, tarball_filename,
                             checksum_filename, product_id,
                             date_generated):
    """ Updates the product_data.txt file with tarball and checksum file info

        Parameters:
            source_dir: Directory where initial product_data.txt file resides
            tarball_filename: Name of tarball
            checksum_filename: Name of checksum file
            product_id: Level 2 product Id
            date_generated: Datetime object indicating the date the L2 product
                            was generated
    """
    product_data_filename = 'product_data.txt'

    # Get size of tarball
    tarball_st = os.stat(tarball_filename)
    tarball_size = tarball_st.st_size

    # Get date for tarball
    tarball_date = dt.datetime.utcfromtimestamp(tarball_st.st_mtime)

    # Get the MD5 sum of the tarball
    tarball_md5 = hashlib.md5(open(tarball_filename, 'rb').read()).hexdigest()

    # Get the total size of all files in the tarball
    product_file_size = 0
    for source_file in os.listdir(source_dir):
        product_file_size += os.path.getsize(
            os.path.join(source_dir, source_file))

    # Get size of checksum file
    checksum_st = os.stat(checksum_filename)

    # Get date for checksum file
    checksum_date = dt.datetime.utcfromtimestamp(checksum_st.st_mtime)

    # Read product_data.txt into dict
    product_data = OrderedDict({})
    with open(product_data_filename) as myfile:
        for line in myfile:
            name, var = line.partition("=")[::2]
            product_data[name.strip()] = var.strip()

    # Add additional product information to product data dict
    product_data["LANDSAT_L2_PRODUCT_ID"] = product_id
    product_data["DATE_PRODUCT_GENERATED_L2"] = \
        date_generated.strftime('%Y/%m/%d %H:%M:%S')
    product_data["L2_PRODUCT_FILE_SIZE"] = product_file_size
    product_data["L2_PACKAGE_NAME"] = tarball_filename
    product_data["L2_PACKAGE_FILE_SIZE"] = tarball_size
    product_data["L2_PACKAGE_FILE_DATE"] = \
        tarball_date.strftime('%Y/%m/%d %H:%M:%S')
    product_data["L2_PACKAGE_FILE_CHECKSUM"] = tarball_md5
    product_data["L2_CHECKSUM_FILE_NAME"] = checksum_filename
    product_data["L2_CHECKSUM_FILE_SIZE"] = checksum_st.st_size
    product_data["L2_CHECKSUM_FILE_DATE"] = \
        checksum_date.strftime('%Y/%m/%d %H:%M:%S')

    # Write product data out to intermediate file
    output_filename = "product_data.tmp"
    with open(output_filename, 'w') as outfile:
        for name, var in product_data.items():
            outfile.write("%s=%s\n" % (name, var))

    # Restore original filename
    os.rename(output_filename, product_data_filename)


def read_and_fix_checksum_file(checksum_filename, tarball_filename):
    """ Replace '-' in checksum file with tarball name

        Parameters:
            checksum_filename: Name of checksum file
            tarball_filename: Name of tarball
    """

    output_filename = "checksum_file.tmp"

    with open(checksum_filename, 'r') as infile:
        in_line = infile.read()

    for line in in_line:
        checksum, filename = in_line.split()

    with open(output_filename, 'w') as outfile:
        outfile.write("%s  %s\n" % (checksum, tarball_filename))

    os.rename(output_filename, checksum_filename)


def package_product(source_directory, product_id, date_generated):
    """ Creates tarball for Level 2 products

        Parameters:
            source_directory: Directory containing files included in package
            product_id: Level 2 product Id
            date_generated: Datetime object representing the time the product
                            was packaged
    """

    (tarball_filename, checksum_filename) = create_tarball(
        source_directory, product_id)

    update_product_data_file(
        source_directory, tarball_filename, checksum_filename, product_id,
        date_generated)

    logger.info('Product Package processing complete.')
