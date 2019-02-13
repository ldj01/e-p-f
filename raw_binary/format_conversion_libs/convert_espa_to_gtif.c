/*****************************************************************************
FILE: convert_espa_to_gtif.c

PURPOSE: Contains functions for creating the GeoTIFF products for each of
the bands in the XML file.

PROJECT:  Land Satellites Data System Science Research and Development (LSRD)
at the USGS EROS

LICENSE TYPE:  NASA Open Source Agreement Version 1.3

NOTES:
  1. The XML metadata format written via this library follows the ESPA internal
     metadata format found in ESPA Raw Binary Format v1.0.doc.  The schema for
     the ESPA internal metadata format is available at
     http://espa.cr.usgs.gov/schema/espa_internal_metadata_v1_0.xsd.
*****************************************************************************/
#include <unistd.h>
#include "gctp.h"
#include "ias_l1g.h"
#include "ias_geo.h"
#include "ias_miscellaneous.h"
#include "raw_binary_io.h"
#include "convert_espa_to_gtif.h"

/******************************************************************************
MODULE:  convert_file_using_library

PURPOSE: Converts the internal ESPA raw binary file to GeoTIFF file format
    using the IAS library GeoTIFF support.

RETURNS: SUCCESS/ERROR
******************************************************************************/
static int convert_file_using_library
(
    const Espa_internal_meta_t *xml_metadata, /* I: Source XML metadata */
    char *espa_filename,                      /* I: Input ESPA filename */
    const char *geotiff_filename,             /* I: Output GeoTIFF filename */
    int band_index
)
{
    char errmsg[STR_SIZE];      /* error message */
    char FUNC_NAME[] = "convert_file_using_library";
    int nscas = 1;
    int nlines;
    int nsamps;
    int nbytes;
    int ias_data_type;
    L1GIO *l1g_image;
    L1G_BAND_IO *l1g_band;
    IAS_L1G_FILE_METADATA fmd;
    IAS_L1G_BAND_METADATA bmd;
    FILE *fp_rb = NULL;         /* File pointer for raw binary file */
    unsigned char *image_buffer = NULL;

    memset(&fmd, 0, sizeof(fmd));
    memset(&bmd, 0, sizeof(bmd));

    /* Read raw binary file */
    nlines = xml_metadata->band[band_index].nlines;
    nsamps = xml_metadata->band[band_index].nsamps;

    fp_rb = open_raw_binary(espa_filename, "rb");
    if ( fp_rb == NULL)
    {
        sprintf(errmsg, "Error opening input raw binary file: %s",
            espa_filename);
        error_handler(true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* determing the IAS data type */
    switch (xml_metadata->band[band_index].data_type)
    {
        case ESPA_INT8:
            ias_data_type = IAS_CHAR;
            break;
        case ESPA_UINT8:
            ias_data_type = IAS_BYTE;
            break;
        case ESPA_INT16:
            ias_data_type = IAS_I2;
            break;
        case ESPA_UINT16:
            ias_data_type = IAS_UI2;
            break;
        case ESPA_INT32:
            ias_data_type = IAS_I4;
            break;
        case ESPA_UINT32:
            ias_data_type = IAS_UI4;
            break;
        case ESPA_FLOAT32:
            ias_data_type = IAS_R4;
            break;
        case ESPA_FLOAT64:
            ias_data_type = IAS_R8;
            break;
        default:
            sprintf(errmsg, "Unsupported ESPA data type: %d",
                    xml_metadata->band[band_index].data_type);
            error_handler(true, FUNC_NAME, errmsg);
            close_raw_binary(fp_rb);
            return ERROR;
    }

    /* Get the size of the selected data type */
    if (ias_misc_get_sizeof_data_type(ias_data_type, &nbytes) != SUCCESS)
    {
        sprintf(errmsg, "Error getting size of IAS data type %d",
                ias_data_type);
        error_handler(true, FUNC_NAME, errmsg);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* The projection codes for ESPA match GCTP, so no conversion is
       necessary */
    fmd.projection_code = xml_metadata->global.proj_info.proj_type;

    /* Make sure the projection code is one of the supported ones */
    if (fmd.projection_code != GEO && fmd.projection_code != UTM
        && fmd.projection_code != ALBERS && fmd.projection_code != PS)
    {
        sprintf(errmsg, "Unsupported projection code: %d",
                fmd.projection_code);
        error_handler(true, FUNC_NAME, errmsg);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    fmd.zone_code = xml_metadata->global.proj_info.utm_zone;
    if (xml_metadata->global.proj_info.datum_type == ESPA_WGS84)
        snprintf(fmd.datum, sizeof(fmd.datum), "WGS84");
    else
    {
        sprintf(errmsg, "Unsupported datum: %d",
            xml_metadata->global.proj_info.datum_type);
        error_handler(true, FUNC_NAME, errmsg);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* Since only WGS84 is supported, hard code the WGS84 spheroid since
       it isn't stored in the xml metadata */
    fmd.spheroid_code = GCTP_WGS84;
    snprintf(fmd.projection_units, sizeof(fmd.projection_units),
             xml_metadata->global.proj_info.units);

    /* Set the projection parameters */
    if (fmd.projection_code == PS)
    {
        /* Convert the projection parameters from degrees to DMS */
        if ((ias_geo_convert_deg2dms(
                xml_metadata->global.proj_info.longitude_pole,
                &fmd.projection_parameters[4], "LON") != SUCCESS)
            || (ias_geo_convert_deg2dms(
                xml_metadata->global.proj_info.latitude_true_scale,
                &fmd.projection_parameters[5], "LAT") != SUCCESS))
        {
            error_handler(true, FUNC_NAME,
                "Converting projection parameters from degrees to DMS");
            close_raw_binary(fp_rb);
            return ERROR;
        }
    }
    else if (fmd.projection_code == ALBERS)
    {
        /* Convert the projection parameters from degrees to DMS */
        if ((ias_geo_convert_deg2dms(
                xml_metadata->global.proj_info.standard_parallel1,
                &fmd.projection_parameters[2], "LAT") != SUCCESS)
            || (ias_geo_convert_deg2dms(
                xml_metadata->global.proj_info.standard_parallel2,
                &fmd.projection_parameters[3], "LAT") != SUCCESS)
            || (ias_geo_convert_deg2dms(
                xml_metadata->global.proj_info.central_meridian,
                &fmd.projection_parameters[4], "LON") != SUCCESS)
            || (ias_geo_convert_deg2dms(
                xml_metadata->global.proj_info.origin_latitude,
                &fmd.projection_parameters[5], "LAT") != SUCCESS))
        {
            error_handler(true, FUNC_NAME,
                "Converting projection parameters from degrees to DMS");
            close_raw_binary(fp_rb);
            return ERROR;
        }
    }
    if (fmd.projection_code == PS || fmd.projection_code == ALBERS)
    {
        fmd.projection_parameters[6] =
            xml_metadata->global.proj_info.false_easting;
        fmd.projection_parameters[7] =
            xml_metadata->global.proj_info.false_northing;
    }

    /* Make sure the corners are center based.  If not, exit with an
       error since we're expecting them to all be center based. */
    if (strcmp(xml_metadata->global.proj_info.grid_origin, "CENTER") != 0)
    {
        sprintf(errmsg, "Unsupported corner grid origin: %s",
            xml_metadata->global.proj_info.grid_origin);
        error_handler(true, FUNC_NAME, errmsg);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* Set up the available band metadata fields that will be written
       to the output GeoTIFF file.  Per the comments in the espa_metadata.h
       file, the corners are relative to the center of the pixel, so no
       adjustments are needed to the corners. */
    bmd.band_number = 1;
    snprintf(bmd.band_name, sizeof(bmd.band_name),
             xml_metadata->band[band_index].name);
    bmd.upper_left_x = xml_metadata->global.proj_info.ul_corner[0];
    bmd.upper_left_y = xml_metadata->global.proj_info.ul_corner[1];
    bmd.upper_right_x = xml_metadata->global.proj_info.lr_corner[0];
    bmd.upper_right_y = xml_metadata->global.proj_info.ul_corner[1];
    bmd.lower_left_x = xml_metadata->global.proj_info.ul_corner[0];
    bmd.lower_left_y = xml_metadata->global.proj_info.lr_corner[1];
    bmd.lower_right_x = xml_metadata->global.proj_info.lr_corner[0];
    bmd.lower_right_y = xml_metadata->global.proj_info.lr_corner[1];
    bmd.projection_distance_x = xml_metadata->band[band_index].pixel_size[0];
    bmd.projection_distance_y = xml_metadata->band[band_index].pixel_size[1];

    /* Open geotiff output file */
    l1g_image = ias_l1g_open_image(geotiff_filename, IAS_WRITE);
    if (l1g_image == NULL)
    {
        sprintf(errmsg, "Error opening output GeoTIFF file: %s",
                geotiff_filename);
        error_handler(true, FUNC_NAME, errmsg);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* Set the file metadata */
    if (ias_l1g_set_file_metadata(l1g_image, &fmd) != SUCCESS)
    {
        sprintf(errmsg, "Error setting file metadata in GeoTIFF file: %s",
                geotiff_filename);
        error_handler(true, FUNC_NAME, errmsg);
        ias_l1g_close_image(l1g_image);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* Set the band metadata */
    if (ias_l1g_set_band_metadata(l1g_image, &bmd, 1) != SUCCESS)
    {
        sprintf(errmsg, "Error setting band metadata in GeoTIFF file: %s",
                geotiff_filename);
        error_handler(true, FUNC_NAME, errmsg);
        ias_l1g_close_image(l1g_image);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* Open the output band */
    l1g_band = ias_l1g_open_band(l1g_image, 1, &ias_data_type, &nscas,
            &nlines, &nsamps);
    if (l1g_band == NULL)
    {
        sprintf(errmsg, "Error opening output band in GeoTIFF file: %s",
                geotiff_filename);
        error_handler(true, FUNC_NAME, errmsg);
        ias_l1g_close_image(l1g_image);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* Allocate memory for the imagery */
    image_buffer = malloc(nlines * nsamps * nbytes);
    if (image_buffer == NULL)
    {
        sprintf(errmsg, "Error allocating image buffer for file: %s",
                geotiff_filename);
        error_handler(true, FUNC_NAME, errmsg);
        ias_l1g_close_band(l1g_band);
        ias_l1g_close_image(l1g_image);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* Read the source imagery */
    if (read_raw_binary(fp_rb, nlines, nsamps, nbytes, image_buffer)
            != SUCCESS)
    {
        sprintf(errmsg, "Error reading image data from raw binary file");
        error_handler(true, FUNC_NAME, errmsg);
        free(image_buffer);
        ias_l1g_close_band(l1g_band);
        ias_l1g_close_image(l1g_image);
        close_raw_binary(fp_rb);
        return ERROR;
    }

    /* Close the input file */
    close_raw_binary(fp_rb);
    fp_rb = NULL;

    if (ias_l1g_write_image(l1g_band, 0, 0, 0, nlines, nsamps, image_buffer)
            != SUCCESS)
    {
        sprintf(errmsg, "Error writing image data to GeoTIFF file: %s",
                geotiff_filename);
        error_handler(true, FUNC_NAME, errmsg);
        free(image_buffer);
        ias_l1g_close_band(l1g_band);
        ias_l1g_close_image(l1g_image);
        return ERROR;
    }

    free(image_buffer);
    image_buffer = NULL;

    /* Close the band */
    if (ias_l1g_close_band(l1g_band) != SUCCESS)
    {
        sprintf(errmsg, "Error closing band in to GeoTIFF file: %s",
                geotiff_filename);
        error_handler(true, FUNC_NAME, errmsg);
        ias_l1g_close_image(l1g_image);
        return ERROR;
    }

    /* Close the file */
    if (ias_l1g_close_image(l1g_image) != SUCCESS)
    {
        sprintf(errmsg, "Error closing  GeoTIFF file: %s", geotiff_filename);
        error_handler(true, FUNC_NAME, errmsg);
        return ERROR;
    }

    return SUCCESS;
}

/******************************************************************************
MODULE:  convert_espa_to_gtif

PURPOSE: Converts the internal ESPA raw binary file to GeoTIFF file format.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting to GeoTIFF
SUCCESS         Successfully converted to GeoTIFF

NOTES:
  1. For the WGS84 datum, the IAS library will be used to write the output
     GeoTIFF file.
  2. For other datums (ESPA specific), it will fall back to using gdal to
     convert the image.  When GDAL is used, an associated .tfw (ESRI world
     file) will be generated for each GeoTIFF file.
******************************************************************************/
int convert_espa_to_gtif
(
    char *espa_xml_file,   /* I: input ESPA XML metadata filename */
    char *gtif_file,       /* I: base output GeoTIFF filename */
    bool del_src           /* I: should the source files be removed after
                                 conversion? */
)
{
    char FUNC_NAME[] = "convert_espa_to_gtif";  /* function name */
    char errmsg[STR_SIZE];      /* error message */
    char espa_band[STR_SIZE];   /* name of the input raw binary for this band*/
    char gtif_band[STR_SIZE];   /* name of the output GeoTIFF for this band */
    char hdr_file[STR_SIZE];    /* name of the header file for this band */
    char xml_file[STR_SIZE];    /* new XML file for the GeoTIFF product */
    char source_dir[STR_SIZE] = ""; /* directory location of source bands */
    char *cptr = NULL;          /* pointer to empty space in the band name */
    int i;                      /* looping variable for each band */
    int count;                  /* number of chars copied in snprintf */
    Espa_internal_meta_t xml_metadata;  /* XML metadata structure to be
                                   populated by reading the XML metadata file */

    /* Validate the input metadata file */
    if (validate_xml_file (espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);

    /* Parse the metadata file into our internal metadata structure; also
       allocates space as needed for various pointers in the global and band
       metadata */
    if (parse_metadata (espa_xml_file, &xml_metadata) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Determine if the files are being read from a location other than cwd */
    if (strchr(espa_xml_file, '/') != NULL)
    {
        strncpy(source_dir, espa_xml_file, sizeof(source_dir));
        cptr = strrchr(source_dir, '/');
        *cptr = '\0';
    }

    /* Loop through the bands in the XML file and convert them to GeoTIFF.
       The filenames will have the GeoTIFF base name followed by _ and the
       band name of each band in the XML file.  Blank spaced in the band name
       will be replaced with underscores. */
    for (i = 0; i < xml_metadata.nbands; i++)
    {
        /* Determine the output GeoTIFF band name */
        count = snprintf (gtif_band, sizeof (gtif_band), "%s_%s.TIF",
            gtif_file, xml_metadata.band[i].name);
        if (count < 0 || count >= sizeof (gtif_band))
        {
            sprintf (errmsg, "Overflow of gtif_band string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Determine the input GeoTIFF band name and location */
        if (strcmp(source_dir, "") == 0)
            count = snprintf (espa_band, sizeof(espa_band), "%s",
                xml_metadata.band[i].file_name);
        else
            count = snprintf (espa_band, sizeof(espa_band), "%s/%s",
                source_dir, xml_metadata.band[i].file_name);
        if (count < 0 || count >= sizeof (espa_band))
        {
            sprintf (errmsg, "Overflow of espa_band string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Loop through this filename and replace any occurances of blank
           spaces with underscores */
        while ((cptr = strchr (gtif_band, ' ')) != NULL)
            *cptr = '_';

        /* Convert the files */
        printf ("Converting %s to %s\n", espa_band, gtif_band);

        if (xml_metadata.global.proj_info.datum_type == ESPA_WGS84)
        {
            /* For WGS84, use the IAS library GeoTIFF IO library to convert
               the file */
            if (convert_file_using_library(&xml_metadata, espa_band, gtif_band,
                    i) != SUCCESS)
            {
                sprintf(errmsg, "Converting espa source file %s to GeoTIFF %s",
                        espa_band, gtif_band);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
        else
        {
            /* If not WGS84, fall back to gdal to convert since it isn't being
               run for Landsat production and the IAS library doesn't support
               datums other than WGS84 */
            char gdal_cmd[STR_SIZE];    /* command string for GDAL call */
            char tmpfile[STR_SIZE];     /* filename of file.tif.aux.xml */

            /* Check if the fill value is defined */
            if ((int) xml_metadata.band[i].fill_value == ESPA_INT_META_FILL)
            {
                /* Fill value is not defined so don't write the nodata tag */
                count = snprintf (gdal_cmd, sizeof (gdal_cmd),
                    "gdal_translate -of Gtiff -co \"TFW=YES\" -q %s %s",
                    espa_band, gtif_band);
            }
            else
            {
                /* Fill value is defined so use the nodata tag */
                count = snprintf (gdal_cmd, sizeof (gdal_cmd),
                     "gdal_translate -of Gtiff -a_nodata %ld -co \"TFW=YES\" "
                     "-q %s %s", xml_metadata.band[i].fill_value, espa_band,
                     gtif_band);
            }
            if (count < 0 || count >= sizeof (gdal_cmd))
            {
                sprintf (errmsg, "Overflow of gdal_cmd string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            if (system (gdal_cmd) == -1)
            {
                sprintf (errmsg, "Running gdal_translate: %s", gdal_cmd);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* Remove the {gtif_name}.tif.aux.xml file since it's not needed and
               clutters the results.  Don't worry about testing the unlink
               results.  If it doesn't unlink it's not fatal. */
            count = snprintf (tmpfile, sizeof (tmpfile), "%s.aux.xml",
                gtif_band);
            if (count < 0 || count >= sizeof (tmpfile))
            {
                sprintf (errmsg, "Overflow of tmpfile string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
            unlink (tmpfile);
        }

        /* Remove the source file if specified */
        if (del_src)
        {
            /* .img file */
            printf ("  Removing %s\n", espa_band);
            if (unlink (espa_band) != 0)
            {
                sprintf (errmsg, "Deleting source file: %s", espa_band);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            /* .hdr file */
            count = snprintf (hdr_file, sizeof (hdr_file), "%s", espa_band);
            if (count < 0 || count >= sizeof (hdr_file))
            {
                sprintf (errmsg, "Overflow of hdr_file string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }

            cptr = strrchr (hdr_file, '.');
            strcpy (cptr, ".hdr");
            printf ("  Removing %s\n", hdr_file);
            if (unlink (hdr_file) != 0)
            {
                sprintf (errmsg, "Deleting source file: %s", hdr_file);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }

        /* Update the XML file to use the new GeoTIFF band name */
        strcpy (xml_metadata.band[i].file_name, gtif_band);
    }

    /* Remove the source XML if specified */
    if (del_src)
    {
        printf ("  Removing %s\n", espa_xml_file);
        if (unlink (espa_xml_file) != 0)
        {
            sprintf (errmsg, "Deleting source file: %s", espa_xml_file);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }

    /* Create the XML file for the GeoTIFF product */
    count = snprintf (xml_file, sizeof (xml_file), "%s_gtif.xml", gtif_file);
    if (count < 0 || count >= sizeof (xml_file))
    {
        sprintf (errmsg, "Overflow of xml_file string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Write the new XML file containing the GeoTIFF band names */
    if (write_metadata (&xml_metadata, xml_file) != SUCCESS)
    {
        sprintf (errmsg, "Error writing updated XML for the GeoTIFF product: "
            "%s", xml_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Successful conversion */
    return (SUCCESS);
}

