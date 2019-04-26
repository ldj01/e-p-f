/*****************************************************************************
FILE: convert_lpgs_to_espa.c

PURPOSE: Contains functions for reading LPGS input GeoTIFF products and
writing to ESPA raw binary format.

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
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "convert_lpgs_to_espa.h"

#define MAX_MSG_STR 256  /* max length of string to print in message */

/* Band information structure */
typedef struct band_information
{
    char id[STR_SIZE];              /* band identifier */
    char fname[STR_SIZE];           /* filename */
    char category[STR_SIZE];        /* category, e.g., qa, image */
    char band_num[STR_SIZE];        /* number */
    enum Espa_data_type data_type;  /* data type */
    bool thermal;                   /* Is this band a thermal band? */
    float min;                      /* minimum value */
    float max;                      /* maximum value */
    float gain;                     /* gain value for band radiance
                                       calculations */
    float bias;                     /* bias value for band radiance
                                       calculations */
    float refl_gain;                /* gain value for TOA reflectance
                                       calculations */
    float refl_bias;                /* bias value for TOA reflectance
                                       calculations */
    float k1;                       /* K1 const for brightness temp
                                       calculations */
    float k2;                       /* K2 const for brightness temp
                                       calculations */
} band_information_t;


/* Get the band information structure associated with a specified
   identifier. */
static band_information_t *get_band_info(band_information_t *band_info,
                                         char *id)
{
    char *FUNC_NAME = "get_band_info";
    band_information_t *binfo = band_info;
    char errmsg[STR_SIZE];    /* error message */

    /* Find the band info for this entry by finding the matching
       ID in the array.  If no match is found, return NULL. */
    while (strcmp(binfo->id, ""))
    {
        if (!strcmp(binfo->id, id))
            return binfo;
        binfo++;
    }

    /* No match found. */
    sprintf(errmsg, "Band info not found for ID %s.", id);
    error_handler(true, FUNC_NAME, errmsg);
    return NULL;
}


/******************************************************************************
MODULE:  read_lpgs_mtl

PURPOSE: Read the LPGS MTL metadata file and populate the ESPA internal
metadata structure

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error reading the metadata file
SUCCESS         Successfully populated the ESPA metadata structure

NOTES:
1. The new MTL files contain the gain and bias coefficients for the TOA
   reflectance and brightness temp calculations.  These coefficients are
   parsed and written to our XML metadata file, if they exist.
2. When processing OLI_TIRS stack the 11 image bands first, then add the
   QA band to the list.
******************************************************************************/
int read_lpgs_mtl
(
    char *mtl_file,                  /* I: name of the MTL metadata file to
                                           be read */
    Espa_internal_meta_t *metadata,  /* I/O: input metadata structure to be
                                           populated from the MTL file */
    int *nlpgs_bands,                /* O: number of bands in LPGS product */
    char lpgs_bands[][STR_SIZE]      /* O: array containing the filenames of
                                           the LPGS bands */
)
{
    char FUNC_NAME[] = "read_lpgs_mtl";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char source_dir[STR_SIZE] = ""; /* directory location of source bands */
    int i;                    /* looping variable */
    int count;                /* number of chars copied in snprintf */
    int band_count = 0;       /* count of the bands processed so we don't have
                                 to specify each band number directly, which
                                 get complicated as we are supporting TM, ETM+,
                                 OLI, etc. */
    bool gain_bias_available; /* are the radiance gain/bias values available
                                 in the MTL file? */
    bool refl_gain_bias_available; /* are TOA reflectance gain/bias values and
                                 K1/K2 constants available in the MTL file? */
    FILE *mtl_fptr=NULL;      /* file pointer to the MTL metadata file */
    Espa_global_meta_t *gmeta = &metadata->global;  /* pointer to the global
                                                       metadata structure */
    Espa_band_meta_t tmp_bmeta;    /* temporary storage of the band-related
                                      metadata for reflective bands */
    Espa_band_meta_t tmp_bmeta_th; /* temporary storage of the band-related
                                      metadata for thermal bands */
    Espa_band_meta_t tmp_bmeta_pan; /* temporary storage of the band-
                                       related metadata for pan bands */
    Space_def_t geoloc_def;  /* geolocation space information */
    Geoloc_t *geoloc_map = NULL;  /* geolocation mapping information */
    Geo_bounds_t bounds;     /* image boundary for the scene */
    double ur_corner[2];     /* geographic UR lat, long */
    double ll_corner[2];     /* geographic LL lat, long */

    /* vars used in parameter parsing */
    char buffer[STR_SIZE] = "\0";          /* line buffer from MTL file */
    char *label = NULL;                    /* label value in the line */
    char *tokenptr = NULL;                 /* pointer to process each line */
    char *seperator = "=\" \t";            /* separator string */
    char group[STR_SIZE] = "";             /* current MTL group */
    float fnum;                            /* temporary variable for floating
                                              point numbers */
    band_information_t band_info[MAX_LPGS_BANDS];

    /* Initialize the band info. */
    memset(band_info, 0, sizeof(band_info));

    /* Identify the source data directory */
    if (strchr(mtl_file, '/') != NULL)
    {
        strncpy(source_dir, mtl_file, sizeof(source_dir));
        tokenptr = strrchr(source_dir, '/');
        *tokenptr = '\0';
    }

    /* Open the metadata MTL file with read privelages */
    mtl_fptr = fopen (mtl_file, "r");
    if (mtl_fptr == NULL)
    {
        sprintf (errmsg, "Opening %s for read access.", mtl_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Process the MTL file line by line */
    gain_bias_available = false;
    refl_gain_bias_available = false;
    while (fgets (buffer, STR_SIZE, mtl_fptr) != NULL)
    {
        /* If the last character is the end of line, then strip it off */
        if (buffer[strlen(buffer)-1] == '\n')
            buffer[strlen(buffer)-1] = '\0';

        /* Get string token */
        tokenptr = strtok (buffer, seperator);

        /* Skip lines that don't contain a separator. */
        if (tokenptr == NULL)
            continue;

        /* Set the parameter label (i.e., keyword) and value. */
        label = tokenptr;
        tokenptr = strtok (NULL, seperator);

        /* If a new group is starting, set the group name.  At the end of
           the group, reset the group name to empty. */
        if (!strcmp(label, "GROUP"))
        {
            strcpy(group, tokenptr);
            continue;
        }
        if (!strcmp(label, "END_GROUP"))
        {
            strcpy(group, "");
            continue;
        }

        /* If we hit the END label, we're done reading the file. */
        if (!strcmp (label, "END"))
            break;

        /* Process tokens in each group. */
        
        /* Read processing information. */
        if (!strcmp(group, "LEVEL1_PROCESSING_RECORD"))
        {
            if (!strcmp (label, "PROCESSING_SOFTWARE_VERSION"))
            {
                count = snprintf(tmp_bmeta.app_version,
                                 sizeof(tmp_bmeta.app_version), "%s", tokenptr);
                if (count < 0 || count >= sizeof (tmp_bmeta.app_version))
                {
                    error_handler(true, FUNC_NAME,
                                  "Overflow of tmp_bmeta.app_version");
                    return ERROR;
                }
            }
            else if (!strcmp (label, "DATE_PRODUCT_GENERATED"))
            {
                count = snprintf(gmeta->level1_production_date,
                                 sizeof(gmeta->level1_production_date), "%s",
                                 tokenptr);
                if (count < 0 ||
                    count >= sizeof (gmeta->level1_production_date))
                {
                    error_handler(true, FUNC_NAME,
                                  "Overflow of gmeta->level1_production_date");
                    return ERROR;
                }
            }
        } /* LEVEL1_PROCESSING_RECORD group */

        /* Read image information. */
        else if (!strcmp(group, "IMAGE_ATTRIBUTES"))
        {
            if (!strcmp (label, "SPACECRAFT_ID"))
            {
                if (strcmp (tokenptr, "LANDSAT_9") == 0 ||
                    strcmp (tokenptr, "Landsat9") == 0)
                    strcpy (gmeta->satellite, "LANDSAT_9");
                else if (strcmp (tokenptr, "LANDSAT_8") == 0 ||
                         strcmp (tokenptr, "Landsat8") == 0)
                    strcpy (gmeta->satellite, "LANDSAT_8");
                else if (strcmp (tokenptr, "LANDSAT_7") == 0 ||
                         strcmp (tokenptr, "Landsat7") == 0)
                    strcpy (gmeta->satellite, "LANDSAT_7");
                else if (strcmp (tokenptr, "LANDSAT_5") == 0 ||
                         strcmp (tokenptr, "Landsat5") == 0)
                    strcpy (gmeta->satellite, "LANDSAT_5");
                else if (strcmp (tokenptr, "LANDSAT_4") == 0 ||
                         strcmp (tokenptr, "Landsat4") == 0)
                    strcpy (gmeta->satellite, "LANDSAT_4");
                else
                {
                    sprintf (errmsg, "Unsupported satellite type: %s",
                             tokenptr);
                    error_handler (true, FUNC_NAME, errmsg);
                    return ERROR;
                }
            }
            else if (!strcmp (label, "SENSOR_ID"))
            {
                count = snprintf(gmeta->instrument, sizeof(gmeta->instrument),
                                 "%s", tokenptr);
                if (count < 0 || count >= sizeof (gmeta->instrument))
                {
                    error_handler(true, FUNC_NAME,
                                  "Overflow of gmeta->instrument string");
                    return ERROR;
                }
            }
            else if (!strcmp (label, "DATE_ACQUIRED"))
            {
                count = snprintf(gmeta->acquisition_date,
                                 sizeof(gmeta->acquisition_date), "%s",
                                 tokenptr);
                if (count < 0 || count >= sizeof (gmeta->acquisition_date))
                {
                    error_handler(true, FUNC_NAME,
                                  "Overflow of gmeta->acquisition_date");
                    return ERROR;
                }
            }
            else if (!strcmp (label, "SCENE_CENTER_TIME"))
            {
                count = snprintf(gmeta->scene_center_time,
                                 sizeof(gmeta->scene_center_time), "%s",
                                 tokenptr);
                if (count < 0 || count >= sizeof (gmeta->scene_center_time))
                {
                    error_handler(true, FUNC_NAME,
                                  "Overflow of gmeta->scene_center_time");
                    return ERROR;
                }
            }
            else if (!strcmp (label, "SUN_ELEVATION"))
            {
                sscanf (tokenptr, "%f", &fnum);
                gmeta->solar_zenith = 90.0 - fnum;
            }
            else if (!strcmp (label, "SUN_AZIMUTH"))
                sscanf (tokenptr, "%f", &gmeta->solar_azimuth);
            else if (!strcmp (label, "EARTH_SUN_DISTANCE"))
                sscanf (tokenptr, "%f", &gmeta->earth_sun_dist);
            else if (!strcmp (label, "WRS_PATH"))
                sscanf (tokenptr, "%d", &gmeta->wrs_path);
            else if (!strcmp (label, "WRS_ROW"))
                sscanf (tokenptr, "%d", &gmeta->wrs_row);
        } /* IMAGE_ATTRIBUTES group */

        /* Read projection information. */
        else if (!strcmp(group, "PROJECTION_ATTRIBUTES"))
        {
            if (!strcmp (label, "MAP_PROJECTION"))
            {
                if (!strcmp (tokenptr, "UTM"))
                    gmeta->proj_info.proj_type = GCTP_UTM_PROJ;
                else if (!strcmp (tokenptr, "PS"))
                    gmeta->proj_info.proj_type = GCTP_PS_PROJ;
                else if (!strcmp (tokenptr, "AEA"))  /* ALBERS */
                    gmeta->proj_info.proj_type = GCTP_ALBERS_PROJ;
                else
                {
                    sprintf (errmsg, "Unsupported projection type: %s. "
                             "Only UTM, PS, and ALBERS EQUAL AREA are "
                             "supported for LPGS.", tokenptr);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "DATUM"))
            {
                if (!strcmp (tokenptr, "WGS84"))
                    gmeta->proj_info.datum_type = ESPA_WGS84;
                else
                {
                    sprintf (errmsg, "Unexpected datum type: %s", tokenptr);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
            else if (!strcmp (label, "UTM_ZONE"))
            {
                sscanf (tokenptr, "%d", &gmeta->proj_info.utm_zone);
            }
            else if (!strcmp (label, "GRID_CELL_SIZE_REFLECTIVE"))
            {
                sscanf (tokenptr, "%lf", &tmp_bmeta.pixel_size[0]);
                tmp_bmeta.pixel_size[1] = tmp_bmeta.pixel_size[0];
            }
            else if (!strcmp (label, "GRID_CELL_SIZE_THERMAL"))
            {
                sscanf (tokenptr, "%lf", &tmp_bmeta_th.pixel_size[0]);
                tmp_bmeta_th.pixel_size[1] = tmp_bmeta_th.pixel_size[0];
            }
            else if (!strcmp (label, "GRID_CELL_SIZE_PANCHROMATIC"))
            {
                sscanf (tokenptr, "%lf", &tmp_bmeta_pan.pixel_size[0]);
                tmp_bmeta_pan.pixel_size[1] = tmp_bmeta_pan.pixel_size[0];
            }
            else if (!strcmp (label, "REFLECTIVE_SAMPLES"))
                sscanf (tokenptr, "%d", &tmp_bmeta.nsamps);
            else if (!strcmp (label, "REFLECTIVE_LINES"))
                sscanf (tokenptr, "%d", &tmp_bmeta.nlines);
            else if (!strcmp (label, "THERMAL_SAMPLES"))
                sscanf (tokenptr, "%d", &tmp_bmeta_th.nsamps);
            else if (!strcmp (label, "THERMAL_LINES"))
                sscanf (tokenptr, "%d", &tmp_bmeta_th.nlines);
            else if (!strcmp (label, "PANCHROMATIC_SAMPLES"))
                sscanf (tokenptr, "%d", &tmp_bmeta_pan.nsamps);
            else if (!strcmp (label, "PANCHROMATIC_LINES"))
                sscanf (tokenptr, "%d", &tmp_bmeta_pan.nlines);

            /* PS projection parameters */
            else if (!strcmp (label, "VERTICAL_LON_FROM_POLE"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.longitude_pole);
            else if (!strcmp (label, "TRUE_SCALE_LAT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.latitude_true_scale);
            else if (!strcmp (label, "FALSE_EASTING"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.false_easting);
            else if (!strcmp (label, "FALSE_NORTHING"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.false_northing);

            /* ALBERS projection parameters (in addition to false easting and
               northing under PS proj params) */
            else if (!strcmp (label, "STANDARD_PARALLEL_1_LAT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.standard_parallel1);
            else if (!strcmp (label, "STANDARD_PARALLEL_2_LAT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.standard_parallel2);
            else if (!strcmp (label, "CENTRAL_MERIDIAN_LON"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.central_meridian);
            else if (!strcmp (label, "ORIGIN_LAT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.origin_latitude);

            else if (!strcmp (label, "CORNER_UL_LAT_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->ul_corner[0]);
            else if (!strcmp (label, "CORNER_UL_LON_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->ul_corner[1]);
            else if (!strcmp (label, "CORNER_LR_LAT_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->lr_corner[0]);
            else if (!strcmp (label, "CORNER_LR_LON_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->lr_corner[1]);
            else if (!strcmp (label, "CORNER_UR_LAT_PRODUCT"))
                sscanf (tokenptr, "%lf", &ur_corner[0]);
            else if (!strcmp (label, "CORNER_UR_LON_PRODUCT"))
                sscanf (tokenptr, "%lf", &ur_corner[1]);
            else if (!strcmp (label, "CORNER_LL_LAT_PRODUCT"))
                sscanf (tokenptr, "%lf", &ll_corner[0]);
            else if (!strcmp (label, "CORNER_LL_LON_PRODUCT"))
                sscanf (tokenptr, "%lf", &ll_corner[1]);

            else if (!strcmp (label, "CORNER_UL_PROJECTION_X_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.ul_corner[0]);
            else if (!strcmp (label, "CORNER_UL_PROJECTION_Y_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.ul_corner[1]);
            else if (!strcmp (label, "CORNER_LR_PROJECTION_X_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.lr_corner[0]);
            else if (!strcmp (label, "CORNER_LR_PROJECTION_Y_PRODUCT"))
                sscanf (tokenptr, "%lf", &gmeta->proj_info.lr_corner[1]);
        } /* PROJECTION_ATTRIBUTES group */

        /* Read information from the LEVEL1_PROJECTION_PARAMETERS group. */
        else if (!strcmp(group, "LEVEL1_PROJECTION_PARAMETERS"))
        {
            if (!strcmp (label, "RESAMPLING_OPTION"))
            {
                if (!strcmp (tokenptr, "CUBIC_CONVOLUTION"))
                    tmp_bmeta.resample_method = ESPA_CC;
                else if (!strcmp (tokenptr, "NEAREST_NEIGHBOR"))
                    tmp_bmeta.resample_method = ESPA_NN;
                else if (!strcmp (tokenptr, "BILINEAR"))
                    tmp_bmeta.resample_method = ESPA_BI;
                else
                {
                    sprintf (errmsg, "Unsupported resampling option: %s",
                             tokenptr);
                    error_handler (true, FUNC_NAME, errmsg);
                    return (ERROR);
                }
            }
        } /* LEVEL1_PROJECTION_PARAMETERS group */

        /* Read information from the PRODUCT_CONTENTS group. */
        else if (!strcmp(group, "PRODUCT_CONTENTS"))
        {
            /* Read the band names and identify band-specific metadata
               information. */
            if (!strncmp(label, "FILE_NAME", 9))
            {
                int bnum;
                int vcid = 0;
                band_information_t *binfo = &band_info[band_count];

                /* Band ID is label after "FILE_NAME_". */
                count = snprintf(binfo->id, sizeof(binfo->id), "%s",
                                 &label[10]);
                if (count < 0 || count >= sizeof(binfo->id))
                {
                    sprintf(errmsg, "Overflow of band ID %d", band_count);
                    error_handler(true, FUNC_NAME, errmsg);
                    return ERROR;
                }

                /* The band ID takes different forms based on the sensor.
                   Most are simply "BAND_n".  But ETM+ appends the VCID
                   for the two thermal bands.  The sscanf() function can
                   match as much of the format string as possible, so both
                   syntax can be read.   The function will return values of
                   1 or 2, depending on the format. */
                if (sscanf(binfo->id, "BAND_%d_VCID_%d", &bnum, &vcid) > 0)
                {
                    count = snprintf(binfo->fname, sizeof(binfo->fname), "%s",
                                     tokenptr);
                    if (count < 0 || count >= sizeof(binfo->fname))
                    {
                        sprintf(errmsg, "Overflow of band filename %d",
                                band_count);
                        error_handler(true, FUNC_NAME, errmsg);
                        return ERROR;
                    }
                    strcpy(binfo->category, "image");
                    if (vcid == 0)
                        sprintf(binfo->band_num, "%d", bnum);
                    else
                        sprintf(binfo->band_num, "%d%d", bnum, vcid);
                    binfo->thermal = false;
                    if ((bnum == 6 && (!strcmp(gmeta->instrument, "TM") ||
                                       vcid != 0)) ||
                        bnum > 9)
                        binfo->thermal = true;
                }
                else if (!strcmp(binfo->id, "QUALITY_L1_PIXEL"))
                {
                    count = snprintf(binfo->fname, sizeof(binfo->fname), "%s",
                                     tokenptr);
                    if (count < 0 || count >= sizeof(binfo->fname))
                    {
                        sprintf(errmsg, "Overflow of band filename %d",
                                band_count);
                        error_handler(true, FUNC_NAME, errmsg);
                        return ERROR;
                    }
                    strcpy(binfo->category, "qa");
                    strcpy(binfo->band_num, "bqa_pixel");
                    binfo->thermal = false;
                }
                else if (!strcmp(binfo->id,
                                 "QUALITY_L1_RADIOMETRIC_SATURATION"))
                {
                    count = snprintf(binfo->fname, sizeof(binfo->fname), "%s",
                                     tokenptr);
                    if (count < 0 || count >= sizeof(binfo->fname))
                    {
                        sprintf(errmsg, "Overflow of band filename %d",
                                band_count);
                        error_handler(true, FUNC_NAME, errmsg);
                        return ERROR;
                    }
                    strcpy(binfo->category, "qa");
                    strcpy(binfo->band_num, "bqa_radsat");
                    binfo->thermal = false;
                }
                else if (!strcmp(binfo->id, "ANGLE_SENSOR_AZIMUTH_BAND_4"))
                {
                    count = snprintf(binfo->fname, sizeof(binfo->fname), "%s",
                                     tokenptr);
                    if (count < 0 || count >= sizeof(binfo->fname))
                    {
                        sprintf(errmsg, "Overflow of band filename %d",
                                band_count);
                        error_handler(true, FUNC_NAME, errmsg);
                        return ERROR;
                    }
                    strcpy(binfo->category, "image");
                    strcpy(binfo->band_num, "sensor_azimuth_band4");
                    binfo->thermal = false;
                }
                else if (!strcmp(binfo->id, "ANGLE_SENSOR_ZENITH_BAND_4"))
                {
                    count = snprintf(binfo->fname, sizeof(binfo->fname), "%s",
                                     tokenptr);
                    if (count < 0 || count >= sizeof(binfo->fname))
                    {
                        sprintf(errmsg, "Overflow of band filename %d",
                                band_count);
                        error_handler(true, FUNC_NAME, errmsg);
                        return ERROR;
                    }
                    strcpy(binfo->category, "image");
                    strcpy(binfo->band_num, "sensor_zenith_band4");
                    binfo->thermal = false;
                }
                else if (!strcmp(binfo->id, "ANGLE_SOLAR_AZIMUTH_BAND_4"))
                {
                    count = snprintf(binfo->fname, sizeof(binfo->fname), "%s",
                                     tokenptr);
                    if (count < 0 || count >= sizeof(binfo->fname))
                    {
                        sprintf(errmsg, "Overflow of band filename %d",
                                band_count);
                        error_handler(true, FUNC_NAME, errmsg);
                        return ERROR;
                    }
                    strcpy(binfo->category, "image");
                    strcpy(binfo->band_num, "solar_azimuth_band4");
                    binfo->thermal = false;
                }
                else if (!strcmp(binfo->id, "ANGLE_SOLAR_ZENITH_BAND_4"))
                {
                    count = snprintf(binfo->fname, sizeof(binfo->fname), "%s",
                                     tokenptr);
                    if (count < 0 || count >= sizeof(binfo->fname))
                    {
                        sprintf(errmsg, "Overflow of band filename %d",
                                band_count);
                        error_handler(true, FUNC_NAME, errmsg);
                        return ERROR;
                    }
                    strcpy(binfo->category, "image");
                    strcpy(binfo->band_num, "solar_zenith_band4");
                    binfo->thermal = false;
                }
                else /* File type not of interest, so skip incrementing the
                        band counter. */
                    continue;

                band_count++;  /* increment the band count */
            } /* FILE_NAME block */

            /* Read the data types for each band. */
            else if (!strncmp(label, "DATA_TYPE", 9))
            {
                band_information_t *binfo = get_band_info(band_info,
                                                          &label[10]);
                if (binfo == NULL)
                    return ERROR;

                if (!strcmp(tokenptr, "INT8"))
                    binfo->data_type = ESPA_INT8;
                else if (!strcmp(tokenptr, "UINT8"))
                    binfo->data_type = ESPA_UINT8;
                else if (!strcmp(tokenptr, "INT16"))
                    binfo->data_type = ESPA_INT16;
                else if (!strcmp(tokenptr, "UINT16"))
                    binfo->data_type = ESPA_UINT16;
                else if (!strcmp(tokenptr, "INT32"))
                    binfo->data_type = ESPA_INT32;
                else if (!strcmp(tokenptr, "UINT32"))
                    binfo->data_type = ESPA_UINT32;
                else if (!strcmp(tokenptr, "FLOAT32"))
                    binfo->data_type = ESPA_FLOAT32;
                else if (!strcmp(tokenptr, "FLOAT64"))
                    binfo->data_type = ESPA_FLOAT64;
                else
                {
                    sprintf(errmsg, "Unsupported data type %s.", tokenptr);
                    error_handler(true, FUNC_NAME, errmsg);
                    return ERROR;
                }
            } /* DATA_TYPE block */

            /* Read the product ID. */
            else if (!strcmp(label, "LANDSAT_PRODUCT_ID"))
            {
                count = snprintf(metadata->global.product_id,
                                 sizeof(metadata->global.product_id),
                                 "%s", tokenptr);
                if (count < 0 || count >= sizeof (metadata->global.product_id))
                {
                    error_handler (true, FUNC_NAME, "Overflow of "
                                   "xml_metadata.global.product_id string");
                    return ERROR;
                }
            }
            else if (!strcmp (label, "PROCESSING_LEVEL"))
            {
                count = snprintf(tmp_bmeta.product, sizeof (tmp_bmeta.product),
                                 "%s", tokenptr);
                if (count < 0 || count >= sizeof (tmp_bmeta.product))
                {
                    error_handler (true, FUNC_NAME,
                                   "Overflow of tmp_bmeta.product string");
                    return ERROR;
                }
            }
       } /* PRODUCT_CONTENTS group */

        /* Read the min and max pixel values. */
        else if (!strcmp(group, "LEVEL1_MIN_MAX_PIXEL_VALUE"))
        {
            /* Parameter syntax: "QUANTIZE_CAL_{MIN|MAX}_BAND_N[_VCID_M]" */
            if (!strncmp(label, "QUANTIZE_CAL", 12))
            {
                int string_offset = 13; /* offset to information embedded in
                                           label */
                int val;                /* parameter value */
                band_information_t *binfo =
                        get_band_info(band_info, &label[string_offset+4]);
                if (binfo == NULL)
                    return ERROR;

                /* Read the max/min value. */
                if (sscanf(tokenptr, "%d", &val) != 1)
                {
                    sprintf(errmsg, "CAL MIN/MAX not readable from %s = %s.",
                            label, tokenptr);
                    error_handler(true, FUNC_NAME, errmsg);
                    return ERROR;
                }

                if (!strncmp(&label[string_offset], "MIN", 3))
                    binfo->min = (float)val;
                else
                    binfo->max = (float)val;
            }
        } /* LEVEL1_MIN_MAX_PIXEL_VALUE group */

        /* Read the radiometric scaline parameters. */
        else if (!strcmp(group, "LEVEL1_RADIOMETRIC_RESCALING"))
        {
            /* Read the radiance gains */
            if (!strncmp(label, "RADIANCE_MULT", 13))
            {
                int string_offset = 14; /* offset to information embedded in
                                           label */
                band_information_t *binfo =
                        get_band_info(band_info, &label[string_offset]);
                if (binfo == NULL)
                    return ERROR;

                if (sscanf(tokenptr, "%f", &binfo->gain) != 1)
                {
                    sprintf(errmsg, "RADIANCE_MULT not readable from %s = %s.",
                            label, tokenptr);
                    error_handler(true, FUNC_NAME, errmsg);
                    return ERROR;
                }

                gain_bias_available = true;
            }

            /* Read the radiance biases */
            else if (!strncmp(label, "RADIANCE_ADD", 12))
            {
                int string_offset = 13; /* offset to information embedded in
                                           label */
                band_information_t *binfo =
                        get_band_info(band_info, &label[string_offset]);
                if (binfo == NULL)
                    return ERROR;

                if (sscanf(tokenptr, "%f", &binfo->bias) != 1)
                {
                    sprintf(errmsg, "RADIANCE_ADD not readable from %s = %s.",
                            label, tokenptr);
                    error_handler(true, FUNC_NAME, errmsg);
                    return ERROR;
                }
            }

            /* Read the reflectance gains */
            else if (!strncmp(label, "REFLECTANCE_MULT", 16))
            {
                int string_offset = 17; /* offset to information embedded in
                                           label */
                band_information_t *binfo =
                        get_band_info(band_info, &label[string_offset]);
                if (binfo == NULL)
                    return ERROR;

                if (sscanf(tokenptr, "%f", &binfo->refl_gain) != 1)
                {
                    sprintf(errmsg, "REFLECTANCE_MULT not readable from "
                            "%s = %s.", label, tokenptr);
                    error_handler(true, FUNC_NAME, errmsg);
                    return ERROR;
                }

                refl_gain_bias_available = true;
            }

            /* Read the reflectance biases */
            else if (!strncmp(label, "REFLECTANCE_ADD", 15))
            {
                int string_offset = 16; /* offset to information embedded in
                                           label */
                band_information_t *binfo =
                        get_band_info(band_info, &label[string_offset]);
                if (binfo == NULL)
                    return ERROR;

                if (sscanf(tokenptr, "%f", &binfo->refl_bias) != 1)
                {
                    sprintf(errmsg, "REFLECTANCE_ADD not readable from "
                            "%s = %s.", label, tokenptr);
                    error_handler(true, FUNC_NAME, errmsg);
                    return ERROR;
                }
            }
        } /* LEVEL1_RADIOMETRIC_RESCALING group */

        /* Read the K1, K2 constants */
        else if (!strcmp(group, "LEVEL1_TIRS_THERMAL_CONSTANTS") ||
                 !strcmp(group, "LEVEL1_THERMAL_CONSTANTS"))
        {
            if (!strncmp(label, "K1_CONSTANT", 11) ||
                !strncmp(label, "K2_CONSTANT", 11))
            {
                int string_offset = 12; /* offset to information embedded in
                                           label */
                float val;              /* parameter value */
                band_information_t *binfo =
                        get_band_info(band_info, &label[string_offset]);
                if (binfo == NULL)
                    return ERROR;

                if (sscanf(tokenptr, "%f", &val) != 1)
                {
                    sprintf(errmsg, "K1/2 constant not readable from %s = %s.",
                            label, tokenptr);
                    error_handler(true, FUNC_NAME, errmsg);
                    return ERROR;
                }
                if (label[1] == '1')
                    binfo->k1 = val;
                else
                    binfo->k2 = val;
            }
        } /* LEVEL1 thermal constants group */
    }  /* end while fgets */

    /* Ensure that the SENSOR_ID is valid for the SPACECRAFT_ID */
    if (strcmp(gmeta->satellite, "LANDSAT_9") == 0
        || strcmp(gmeta->satellite, "LANDSAT_8") == 0)
    {
        if (strcmp (gmeta->instrument, "OLI_TIRS") != 0
            && strcmp (gmeta->instrument, "OLI") != 0
            && strcmp (gmeta->instrument, "TIRS") != 0)
        {
            sprintf (errmsg, "Unsupported sensor type: %.*s", MAX_MSG_STR,
                gmeta->instrument);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }
    else if (strcmp(gmeta->satellite, "LANDSAT_7") == 0)
    {
        if (strcmp (gmeta->instrument, "ETM") != 0)
        {
            sprintf (errmsg, "Unsupported sensor type: %.*s", MAX_MSG_STR,
                gmeta->instrument);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }
    else if (strcmp(gmeta->satellite, "LANDSAT_5") == 0
        || strcmp(gmeta->satellite, "LANDSAT_4") == 0)
    {
        if (strcmp (gmeta->instrument, "TM") != 0)
        {
            sprintf (errmsg, "Unsupported sensor type: %.*s", MAX_MSG_STR,
                gmeta->instrument);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
    }
    else  /* SPACECRAFT_ID not populated */
    {
        sprintf (errmsg,
            "SPACECRAFT_ID is required to validate SENSOR_ID");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Check the band count to make sure we didn't go over the maximum
       expected */
    if (band_count > MAX_LPGS_BANDS)
    {
        sprintf (errmsg, "The total band count of LPGS bands converted for "
            "this product (%d) exceeds the maximum expected (%d).", band_count,
            MAX_LPGS_BANDS);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Set defaults that aren't in the MTL file */
    gmeta->wrs_system = 2;
    gmeta->orientation_angle = 0.0;
    strcpy (gmeta->data_provider, "USGS/EROS");
    strcpy (gmeta->solar_units, "degrees");

    count = snprintf (gmeta->lpgs_metadata_file,
        sizeof (gmeta->lpgs_metadata_file), "%s", mtl_file);
    if (count < 0 || count >= sizeof (gmeta->lpgs_metadata_file))
    {
        sprintf (errmsg, "Overflow of gmeta->lpgs_metadata_file string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    count = snprintf (gmeta->proj_info.units, sizeof (gmeta->proj_info.units),
        "%s", "meters");
    if (count < 0 || count >= sizeof (gmeta->proj_info.units))
    {
        sprintf (errmsg, "Overflow of gmeta->proj_info.units string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* UL and LR corner projection coords in the MTL file are for the center
       of the pixel.  Given there are different resolution bands, leave the
       corners as the center of the pixel. */
    strcpy (gmeta->proj_info.grid_origin, "CENTER");

    /* Set up the number of total bands */
    metadata->nbands = band_count;
    if (allocate_band_metadata (metadata, metadata->nbands) != SUCCESS)
    {   /* Error messages already printed */
        return (ERROR);
    }

    /* Fill in the band-related metadata for each of the bands */
    *nlpgs_bands = metadata->nbands;
    for (i = 0; i < metadata->nbands; i++)
    {
        Espa_band_meta_t *bmeta = &metadata->band[i];  /* pointer to the array
                                                          of bands metadata */
        band_information_t *binfo = &band_info[i];

        /* Handle the general metadata for each band */
        if (strcmp(source_dir, "") == 0)
            count = snprintf (lpgs_bands[i], sizeof (lpgs_bands[i]), "%s",
                binfo->fname);
        else
            count = snprintf (lpgs_bands[i], sizeof (lpgs_bands[i]), "%s/%s",
                source_dir, binfo->fname);

        if (count < 0 || count >= sizeof (lpgs_bands[i]))
        {
            sprintf (errmsg, "Overflow of lpgs_bands[i] string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Use Level 1 product type for all bands */
        count = snprintf (bmeta->product, sizeof (bmeta->product), "%s",
            tmp_bmeta.product);
        if (count < 0 || count >= sizeof (bmeta->product))
        {
            sprintf (errmsg, "Overflow of bmeta->product string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta->category, sizeof (bmeta->category), "%s",
            binfo->category);
        if (count < 0 || count >= sizeof (bmeta->category))
        {
            sprintf (errmsg, "Overflow of bmeta->category string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta->app_version, sizeof (bmeta->app_version),
            "%s", tmp_bmeta.app_version);
        if (count < 0 || count >= sizeof (bmeta->app_version))
        {
            sprintf (errmsg, "Overflow of bmeta->app_version string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        bmeta->valid_range[0] = binfo->min;
        bmeta->valid_range[1] = binfo->max;

        if (gain_bias_available)
        {
            bmeta->rad_gain = binfo->gain;
            bmeta->rad_bias = binfo->bias;
        }

        if (refl_gain_bias_available)
        {
            /* Gain/bias only exist for image bands */
            if (!strcmp (binfo->category, "image"))
            {
                /* Reflectance gain/bias values don't exist for the thermal
                   bands, but the K constants do */
                if (binfo->thermal)
                {
                    bmeta->k1_const = binfo->k1;
                    bmeta->k2_const = binfo->k2;
                }
                else
                {
                    bmeta->refl_gain = binfo->refl_gain;
                    bmeta->refl_bias = binfo->refl_bias;
                }
            }
            else
            {
                /* QA bands don't have these */
                bmeta->refl_gain = ESPA_FLOAT_META_FILL;
                bmeta->refl_bias = ESPA_FLOAT_META_FILL;
                bmeta->k1_const = ESPA_FLOAT_META_FILL;
                bmeta->k2_const = ESPA_FLOAT_META_FILL;
            }
        }

        count = snprintf (bmeta->data_units, sizeof (bmeta->data_units),
            "%s", "digital numbers");
        if (count < 0 || count >= sizeof (bmeta->data_units))
        {
            sprintf (errmsg, "Overflow of bmeta->data_units string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta->pixel_units, sizeof (bmeta->pixel_units),
            "%s", "meters");
        if (count < 0 || count >= sizeof (bmeta->pixel_units))
        {
            sprintf (errmsg, "Overflow of bmeta->pixel_units string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        count = snprintf (bmeta->production_date,
            sizeof (bmeta->production_date), "%s",
            gmeta->level1_production_date);
        if (count < 0 || count >= sizeof (bmeta->production_date))
        {
            sprintf (errmsg, "Overflow of bmeta->production_date string");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        bmeta->resample_method = tmp_bmeta.resample_method;
        bmeta->data_type = binfo->data_type;
        if (!strcmp (gmeta->instrument, "TM"))
        {
            bmeta->fill_value = 0;
            if (!strcmp (gmeta->satellite, "LANDSAT_4"))
                strcpy (bmeta->short_name, "LT04");
            else if (!strcmp (gmeta->satellite, "LANDSAT_5"))
                strcpy (bmeta->short_name, "LT05");
        }
        else if (!strncmp (gmeta->instrument, "ETM", 3))
        {
            bmeta->fill_value = 0;
            strcpy (bmeta->short_name, "LE07");
        }
        else if (!strcmp (gmeta->instrument, "OLI_TIRS")
             ||  !strcmp (gmeta->instrument, "OLI")
             ||  !strcmp (gmeta->instrument, "TIRS"))
        {
            bmeta->fill_value = 0;
            if (!strcmp (gmeta->satellite, "LANDSAT_8"))
                strcpy (bmeta->short_name, "LC08");
            else if (!strcmp (gmeta->satellite, "LANDSAT_9"))
                strcpy (bmeta->short_name, "LC09");
        }

        /* Set up the band names - use lower case 'b' versus upper case 'B'
           to distinguish ESPA products from original Level-1 products. */
        if (sscanf(binfo->band_num, "%d", &count) == 1)
        { /* band numbers */
            sprintf (bmeta->name, "b%s", binfo->band_num);
            sprintf (bmeta->long_name, "band %s digital numbers",
              binfo->band_num);
            strcat (bmeta->short_name, "DN");
        }
        else
        {
            strcpy (bmeta->name, binfo->band_num);
            if (strstr(bmeta->name, "bqa_pixel"))
            {
                strcpy (bmeta->long_name, "pixel quality band");
                strcat (bmeta->short_name, "PQA");
            }
            else if (strstr(bmeta->name, "bqa_radsat"))
            {
                strcpy (bmeta->long_name, "saturation quality band");
                strcat (bmeta->short_name, "RADSAT");
            }
            else if (strstr(bmeta->name, "sensor_azimuth_band4"))
            {
                strcpy (bmeta->long_name, "band 4 sensor azimuth angles");
                strcat (bmeta->short_name, "SENAZ");
            }
            else if (strstr(bmeta->name, "sensor_zenith_band4"))
            {
                strcpy (bmeta->long_name, "band 4 sensor zenith angles");
                strcat (bmeta->short_name, "SENZEN");
            }
            else if (strstr(bmeta->name, "solar_azimuth_band4"))
            {
                strcpy (bmeta->long_name, "band 4 solar azimuth angles");
                strcat (bmeta->short_name, "SOLAZ");
            }
            else if (strstr(bmeta->name, "solar_zenith_band4"))
            {
                strcpy (bmeta->long_name, "band 4 solar zenith angles");
                strcat (bmeta->short_name, "SOLZEN");
            }
        }

        count = snprintf (bmeta->file_name, sizeof (bmeta->file_name),
            "%s_%s.img", metadata->global.product_id, bmeta->name);
        if (count < 0 || count >= sizeof (bmeta->file_name))
        {
            sprintf (errmsg, "Overflow of bmeta->file_name");
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }

        /* Set up the image size and resolution */
        if (binfo->thermal)
        {  /* thermal bands */
            bmeta->nlines = tmp_bmeta_th.nlines;
            bmeta->nsamps = tmp_bmeta_th.nsamps;
            bmeta->pixel_size[0] = tmp_bmeta_th.pixel_size[0];
            bmeta->pixel_size[1] = tmp_bmeta_th.pixel_size[1];
        }
        else if (!strcmp (binfo->band_num, "8"))
        {  /* pan bands - both ETM+ and OLI band 8 are pan bands */
            bmeta->nlines = tmp_bmeta_pan.nlines;
            bmeta->nsamps = tmp_bmeta_pan.nsamps;
            bmeta->pixel_size[0] = tmp_bmeta_pan.pixel_size[0];
            bmeta->pixel_size[1] = tmp_bmeta_pan.pixel_size[1];
        }
        else
        {  /* all other bands */
            bmeta->nlines = tmp_bmeta.nlines;
            bmeta->nsamps = tmp_bmeta.nsamps;
            bmeta->pixel_size[0] = tmp_bmeta.pixel_size[0];
            bmeta->pixel_size[1] = tmp_bmeta.pixel_size[1];
        }

        /* If this is a QA band, then overwrite some things. */
        if (!strncmp(binfo->band_num, "bqa", 3))
        {
            if (!strcmp(binfo->band_num, "bqa_radsat"))
                count = snprintf(bmeta->data_units, sizeof(bmeta->data_units),
                                 "%s", "bitmap");
            else
                count = snprintf(bmeta->data_units, sizeof(bmeta->data_units),
                                 "%s", "quality/feature classification");
            if (count < 0 || count >= sizeof (bmeta->data_units))
            {
                error_handler(true, FUNC_NAME,
                              "Overflow of bmeta->data_units string");
                return ERROR;
            }

            bmeta->valid_range[0] = 0.0;
            bmeta->valid_range[1] = 65535.0;
            bmeta->rad_gain = ESPA_FLOAT_META_FILL;
            bmeta->rad_bias = ESPA_FLOAT_META_FILL;

            if (allocate_bitmap_metadata(bmeta, 16) != SUCCESS)
            {
                error_handler(true, FUNC_NAME,
                              "Allocating 16 bits for the bitmap");
                return ERROR;
            }

            /* Set band-specific information. */
            if (!strcmp(binfo->band_num, "bqa"))
            {
                strcpy(bmeta->bitmap_description[0],
                       "Data Fill Flag (0 = valid data, 1 = invalid data)");
                if (!strncmp(gmeta->instrument, "OLI", 3))
                {  /* OLI */
                    strcpy(bmeta->bitmap_description[1],
                           "Terrain Occlusion (0 = not terrain occluded, "
                           "1 = terrain occluded)");
                }
                else
                {  /* TM/ETM+ */
                    strcpy(bmeta->bitmap_description[1], "Dropped Pixel "
                           "(0 = not a dropped pixel , 1 = dropped pixel)");
                }
                strcpy(bmeta->bitmap_description[2], "Radiometric Saturation");
                strcpy(bmeta->bitmap_description[3], "Radiometric Saturation");
                strcpy(bmeta->bitmap_description[4], "Cloud");
                strcpy(bmeta->bitmap_description[5], "Cloud Confidence");
                strcpy(bmeta->bitmap_description[6], "Cloud Confidence");
                strcpy(bmeta->bitmap_description[7], "Cloud Shadow Confidence");
                strcpy(bmeta->bitmap_description[8], "Cloud Shadow Confidence");
                strcpy(bmeta->bitmap_description[9], "Snow/Ice Confidence");
                strcpy(bmeta->bitmap_description[10], "Snow/Ice Confidence");
                if (!strncmp(gmeta->instrument, "OLI", 3))
                {  /* OLI */
                    strcpy(bmeta->bitmap_description[11], "Cirrus Confidence");
                    strcpy(bmeta->bitmap_description[12], "Cirrus Confidence");
                }
                else
                {  /* TM/ETM+ */
                    strcpy(bmeta->bitmap_description[11], "Not used");
                    strcpy(bmeta->bitmap_description[12], "Not used");
                }
                strcpy(bmeta->bitmap_description[13], "Not used");
                strcpy(bmeta->bitmap_description[14], "Not used");
                strcpy(bmeta->bitmap_description[15], "Not used");
            } /* bqa band */
            else if (!strcmp(binfo->band_num, "bqa_pixel"))
            {
                strcpy(bmeta->bitmap_description[0],
                       "Data Fill Flag (0 = valid data, 1 = invalid data)");
                strcpy(bmeta->bitmap_description[1], "Dilated Cloud");
                strcpy(bmeta->bitmap_description[2], "Cirrus");
                strcpy(bmeta->bitmap_description[3], "Cloud");
                strcpy(bmeta->bitmap_description[4], "Cloud Shadow");
                strcpy(bmeta->bitmap_description[5], "Snow");
                strcpy(bmeta->bitmap_description[6], "Clear");
                strcpy(bmeta->bitmap_description[7], "Water");
                strcpy(bmeta->bitmap_description[8], "Cloud Confidence");
                strcpy(bmeta->bitmap_description[9], "Cloud Confidence");
                strcpy(bmeta->bitmap_description[10],
                       "Cloud Shadow Confidence");
                strcpy(bmeta->bitmap_description[11],
                       "Cloud Shadow Confidence");
                strcpy(bmeta->bitmap_description[12], "Snow/Ice Confidence");
                strcpy(bmeta->bitmap_description[13], "Snow/Ice Confidence");
                if (!strncmp(gmeta->instrument, "OLI", 3))
                {  /* OLI */
                    strcpy(bmeta->bitmap_description[14], "Cirrus Confidence");
                    strcpy(bmeta->bitmap_description[15], "Cirrus Confidence");
                }
                else
                {  /* TM/ETM+ */
                    strcpy(bmeta->bitmap_description[14], "Not used");
                    strcpy(bmeta->bitmap_description[15], "Not used");
                }
            } /* bqa pixel band */
            else if (!strcmp(binfo->band_num, "bqa_radsat"))
            {
                int bit;
                for (bit = 0; bit < 8; bit++)
                    sprintf(bmeta->bitmap_description[bit],
                            "Band %d Saturation", bit + 1);
                if (!strncmp(gmeta->instrument, "OLI", 3))
                {  /* OLI */
                    strcpy(bmeta->bitmap_description[8], "Band 9 Saturation");
                    strcpy(bmeta->bitmap_description[9], "Band 10 Saturation");
                    strcpy(bmeta->bitmap_description[10], "Band 11 Saturation");
                    strcpy(bmeta->bitmap_description[11], "Terrain Occlusion");
                }
                else
                {  /* TM/ETM+ */
                    strcpy(bmeta->bitmap_description[8], "Band 6H Saturation");
                    strcpy(bmeta->bitmap_description[9], "Dropped Pixel");
                    strcpy(bmeta->bitmap_description[10], "Not used");
                    strcpy(bmeta->bitmap_description[11], "Not used");
                }
                for (bit = 12; bit < 16; bit++)
                    strcpy(bmeta->bitmap_description[bit], "Not used");
            } /* bqa radsat band */
        } /* QA  bands */

        /* Collection 2 angle bands */
        else if (strstr (binfo->band_num, "zenith") ||
                 strstr (binfo->band_num, "azimuth"))
        {
            bmeta->scale_factor = 0.01;
            bmeta->add_offset = 0.00;
            /* Set the valid range for azimuth / zenith */
            float min_angle = (strstr (binfo->band_num, "zenith")) ? 0 : -180;
            bmeta->valid_range[0] = min_angle / bmeta->scale_factor
                + bmeta->add_offset;
            bmeta->valid_range[1] = 180.0 / bmeta->scale_factor
                + bmeta->add_offset;
            bmeta->rad_gain = ESPA_FLOAT_META_FILL;
            bmeta->rad_bias = ESPA_FLOAT_META_FILL;
            count = snprintf (bmeta->data_units, sizeof (bmeta->data_units),
                "%s", "degrees");
            if (count < 0 || count >= sizeof (bmeta->data_units))
            {
                sprintf (errmsg, "Overflow of bmeta->data_units string");
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
    } /* band loop */

    /* Close the metadata file */
    fclose (mtl_fptr);

    /* Get geolocation information from the XML file to prepare for computing
       the bounding coordinates */
    if (!get_geoloc_info (metadata, &geoloc_def))
    {
        sprintf (errmsg, "Copying the geolocation information from the XML "
            "metadata structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Setup the mapping structure */
    geoloc_map = setup_mapping (&geoloc_def);
    if (geoloc_map == NULL)
    {
        sprintf (errmsg, "Setting up the geolocation mapping structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Compute the geographic bounds using the reflectance band coordinates */
    /* For ascending scenes and scenes in the polar regions, the scenes are
       flipped upside down.  The bounding coords will be correct in North
       represents the northernmost latitude and South represents the
       southernmost latitude.  However, the UL corner in this case would be
       more south than the LR corner.  Comparing the UL and LR corners will
       allow the user to determine if the scene is flipped. */
    if (!compute_bounds (geoloc_map, tmp_bmeta.nlines, tmp_bmeta.nsamps,
        &bounds))
    {
        sprintf (errmsg, "Setting up the geolocation mapping structure.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    gmeta->bounding_coords[ESPA_WEST] = bounds.min_lon;
    gmeta->bounding_coords[ESPA_EAST] = bounds.max_lon;
    gmeta->bounding_coords[ESPA_NORTH] = bounds.max_lat;
    gmeta->bounding_coords[ESPA_SOUTH] = bounds.min_lat;

    /* Free the geolocation structure */
    free (geoloc_map);

    /* Successful read */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  convert_gtif_to_img

PURPOSE: Convert the LPGS GeoTIFF band to ESPA raw binary (.img) file and
writes the associated ENVI header for each band.

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting the GeoTIFF file
SUCCESS         Successfully converted GeoTIFF to raw binary

NOTES:
1. TIFF read scanline only supports reading a single line at a time.  We will
   read a single line, stuff it into a large buffer, then write the entire
   image at one time.  This is about 40% faster than reading a single line
   then writing a single line.
******************************************************************************/
int convert_gtif_to_img
(
    char *gtif_file,           /* I: name of the input GeoTIFF file */
    Espa_band_meta_t *bmeta,   /* I: pointer to band metadata for this band */
    Espa_global_meta_t *gmeta  /* I: pointer to global metadata */
)
{
    char FUNC_NAME[] = "convert_gtif_to_img";  /* function name */
    char errmsg[STR_SIZE];    /* error message */
    char *cptr = NULL;        /* pointer to the file extension */
    char *img_file = NULL;    /* name of the output raw binary file */
    char envi_file[STR_SIZE]; /* name of the output ENVI header file */
    int i;                    /* looping variable for lines in image */
    int nbytes;               /* number of bytes in the data type */
    int count;                /* number of chars copied in snprintf */
    void *file_buf = NULL;    /* pointer to correct input file buffer */
    uint8 *file_buf_u8 = NULL;  /* buffer for uint8 TIFF data to be read */
    int16 *file_buf_i16 = NULL; /* buffer for int16 TIFF data to be read */
    int16 *file_buf_u16 = NULL; /* buffer for uint16 TIFF data to be read */
    TIFF *fp_tiff = NULL;     /* file pointer for the TIFF file */
    FILE *fp_rb = NULL;       /* file pointer for the raw binary file */
    Envi_header_t envi_hdr;   /* output ENVI header information */

    /* Open the TIFF file for reading */
    fp_tiff = XTIFFOpen (gtif_file, "r");
    if (fp_tiff == NULL)
    {
        sprintf (errmsg, "Opening the LPGS GeoTIFF file: %s", gtif_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Open the raw binary file for writing */
    img_file = bmeta->file_name;
    fp_rb = open_raw_binary (img_file, "wb");
    if (fp_rb == NULL)
    {
        sprintf (errmsg, "Opening the output raw binary file: %s", img_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Allocate memory for the entire image, based on the input data type */
    if (bmeta->data_type == ESPA_UINT8)
    {
        nbytes = sizeof (uint8);
        file_buf_u8 = calloc (bmeta->nlines * bmeta->nsamps, nbytes);
        if (file_buf_u8 == NULL)
        {
            sprintf (errmsg, "Allocating memory for the image of uint8 data "
                "containing %d lines x %d samples.", bmeta->nlines,
                bmeta->nsamps);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        file_buf = file_buf_u8;
    }
    else if (bmeta->data_type == ESPA_INT16)
    {
        nbytes = sizeof (int16);
        file_buf_i16 = calloc (bmeta->nlines * bmeta->nsamps, nbytes);
        if (file_buf_i16 == NULL)
        {
            sprintf (errmsg, "Allocating memory for the image of int16 data "
                "containing %d lines x %d samples.", bmeta->nlines,
                bmeta->nsamps);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        file_buf = file_buf_i16;
    }
    else if (bmeta->data_type == ESPA_UINT16)
    {
        nbytes = sizeof (uint16);
        file_buf_u16 = calloc (bmeta->nlines * bmeta->nsamps, nbytes);
        if (file_buf_u16 == NULL)
        {
            sprintf (errmsg, "Allocating memory for the image of uint16 data "
                "containing %d lines x %d samples.", bmeta->nlines,
                bmeta->nsamps);
            error_handler (true, FUNC_NAME, errmsg);
            return (ERROR);
        }
        file_buf = file_buf_u16;
    }
    else
    {
        sprintf (errmsg, "Unsupported data type.  Currently only uint8, "
            "int16, and uint16 are supported.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Loop through the lines in the TIFF file, reading and stuffing in the
       image buffer */
    if (bmeta->data_type == ESPA_UINT8)
    {
        for (i = 0; i < bmeta->nlines; i++)
        {
            /* Read current line from the TIFF file */
            if (!TIFFReadScanline (fp_tiff, &file_buf_u8[i*bmeta->nsamps],
                i, 0))
            {
                sprintf (errmsg, "Reading line %d from the TIFF file: %s", i,
                    gtif_file);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
    }
    else if (bmeta->data_type == ESPA_INT16)
    {
        for (i = 0; i < bmeta->nlines; i++)
        {
            /* Read current line from the TIFF file */
            if (!TIFFReadScanline (fp_tiff, &file_buf_i16[i*bmeta->nsamps],
                i, 0))
            {
                sprintf (errmsg, "Reading line %d from the TIFF file: %s", i,
                    gtif_file);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
    }
    else if (bmeta->data_type == ESPA_UINT16)
    {
        for (i = 0; i < bmeta->nlines; i++)
        {
            /* Read current line from the TIFF file */
            if (!TIFFReadScanline (fp_tiff, &file_buf_u16[i*bmeta->nsamps],
                i, 0))
            {
                sprintf (errmsg, "Reading line %d from the TIFF file: %s", i,
                    gtif_file);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
    }

    /* Write entire image to the raw binary file */
    if (write_raw_binary (fp_rb, bmeta->nlines, bmeta->nsamps, nbytes,
        file_buf) != SUCCESS)
    {
        sprintf (errmsg, "Writing image to the raw binary file: %s", img_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Close the TIFF and raw binary files */
    XTIFFClose (fp_tiff);
    close_raw_binary (fp_rb);

    /* Free the memory */
    free (file_buf_u8);
    free (file_buf_i16);
    free (file_buf_u16);

    /* Create the ENVI header file this band */
    if (create_envi_struct (bmeta, gmeta, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Creating the ENVI header structure for this file.");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Write the ENVI header */
    count = snprintf (envi_file, sizeof (envi_file), "%s", img_file);
    if (count < 0 || count >= sizeof (envi_file))
    {
        sprintf (errmsg, "Overflow of envi_file string");
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }
    cptr = strchr (envi_file, '.');
    strcpy (cptr, ".hdr");

    if (write_envi_hdr (envi_file, &envi_hdr) != SUCCESS)
    {
        sprintf (errmsg, "Writing the ENVI header file: %.*s.", MAX_MSG_STR,
                 envi_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* Successful conversion */
    return (SUCCESS);
}


/******************************************************************************
MODULE:  convert_lpgs_to_espa

PURPOSE: Converts the input LPGS GeoTIFF files (and associated MTL file) to
the ESPA internal raw binary file format (and associated XML file).

RETURN VALUE:
Type = int
Value           Description
-----           -----------
ERROR           Error converting the GeoTIFF file
SUCCESS         Successfully converted GeoTIFF to raw binary

NOTES:
  1. The LPGS GeoTIFF band files will be deciphered from the LPGS MTL file.
  2. The ESPA raw binary band files will be generated from the ESPA XML
     filename.
******************************************************************************/
int convert_lpgs_to_espa
(
    char *lpgs_mtl_file,   /* I: input LPGS MTL metadata filename */
    char *espa_xml_file,   /* I: output ESPA XML metadata filename */
    bool del_src,          /* I: should the source .tif files be removed after
                                 conversion? */
    bool sr_st_only        /* I: only convert bands required for SR/ST */
)
{
    char FUNC_NAME[] = "convert_lpgs_to_espa";  /* function name */
    char errmsg[STR_SIZE];   /* error message */
    Espa_internal_meta_t xml_metadata;  /* XML metadata structure to be
                                populated by reading the MTL metadata file */
    int i,j,x;               /* looping variables */
    int nlpgs_bands;         /* number of bands in the LPGS product */
    int convert_lpgs_bands[MAX_LPGS_BANDS]; /* flag to convert each band */
    char lpgs_bands[MAX_LPGS_BANDS][STR_SIZE];  /* array containing the file
                                names of the LPGS bands */
    char exclude_bands[][STR_SIZE] =  /* bands to exclude, not used in SR/ST */
        {"b62", "b8", "b9",
         "sensor_azimuth_band4", "sensor_zenith_band4", "solar_azimuth_band4"};
    int  nexclude = sizeof(exclude_bands)/STR_SIZE;

    /* Initialize the metadata structure */
    init_metadata_struct (&xml_metadata);
    memset(convert_lpgs_bands, 1, sizeof(convert_lpgs_bands));

    /* Read the LPGS MTL file and populate our internal ESPA metadata
       structure */
    if (read_lpgs_mtl (lpgs_mtl_file, &xml_metadata, &nlpgs_bands,
        lpgs_bands) != SUCCESS)
    {
        sprintf (errmsg, "Reading the LPGS MTL file: %s", lpgs_mtl_file);
        error_handler (true, FUNC_NAME, errmsg);
        return (ERROR);
    }

    /* If requested, remove unneeded bands */
    /* The lpgs_bands list is kept intact (using the i index), while the
       bands in xml_metadata are pared down to only the bands to be kept (using
       the x index) */
    if (sr_st_only)
    {
        for (i = 0, x = 0; i < nlpgs_bands; i++, x++)
        {
            int found_exclude;
            for (found_exclude = 0, j = 0; j < nexclude; j++)
            {
                if (!strcmp(exclude_bands[j], xml_metadata.band[x].name))
                {
                    found_exclude = 1;
                    break;
                }
            }

            /* If this band is in the exclude list, remove from the XML list
               by shifting the rest of the array and decrementing the band
               count */
            if (found_exclude)
            {
                if (i < (nlpgs_bands-1))
                {
                    memmove(&xml_metadata.band[x], &xml_metadata.band[x+1],
                        (nlpgs_bands-i-1)*sizeof(Espa_band_meta_t));
                }
                x--;
                xml_metadata.nbands--;
            }
            convert_lpgs_bands[i] = 1 - found_exclude;
        }
    }

    /* Write the metadata from our internal metadata structure to the output
       XML filename */
    if (write_metadata (&xml_metadata, espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Validate the input metadata file */
    if (validate_xml_file (espa_xml_file) != SUCCESS)
    {  /* Error messages already written */
        return (ERROR);
    }

    /* Convert each of the LPGS GeoTIFF files to raw binary */
    for (i = 0, x = 0; i < nlpgs_bands; i++)
    {
        if (convert_lpgs_bands[i])
        {
            printf ("  Band %d: %s to %s\n", i, lpgs_bands[i],
                    xml_metadata.band[x].file_name);
            if (convert_gtif_to_img (lpgs_bands[i], &xml_metadata.band[x],
                                     &xml_metadata.global) != SUCCESS)
            {
                sprintf (errmsg, "Converting band %d: %.*s", i, MAX_MSG_STR,
                         lpgs_bands[i]);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
            x++;
        }

        /* Remove the source file if specified */
        if (del_src)
        {
            printf ("  Removing %s\n", lpgs_bands[i]);
            if (unlink (lpgs_bands[i]) != 0)
            {
                sprintf (errmsg, "Deleting source file: %.*s", MAX_MSG_STR,
                         lpgs_bands[i]);
                error_handler (true, FUNC_NAME, errmsg);
                return (ERROR);
            }
        }
    }

    /* Free the metadata structure */
    free_metadata (&xml_metadata);

    /* Successful conversion */
    return (SUCCESS);
}
