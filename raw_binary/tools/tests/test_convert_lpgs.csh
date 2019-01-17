#!/bin/csh -f

#***********************************************************************
# NAME:     test_convert_lpgs.csh
# PURPOSE:  tests conversion of LPGS level 1 products
# RETURNS:  0 if successful, 1 on an error condition
#***********************************************************************/

# Get the location of the binary being run
if ($#argv != 1) then
  echo "Usage: $0 bindir"
  exit 1
endif
set bindir = $1

# Ensure the test environment is set
if ( ! $?ESPA_UNIT_TEST_DATA_DIR ) then
    echo "ESPA_UNIT_TEST_DATA_DIR isn't set"
    exit 1
endif

set l7=LE07_L1TP_043028_20020419_20181009_01_T1
set l8=LC08_L1TP_043028_20160401_20181005_02_T1
set tests = "01 02 03 04"

foreach scene ($l7 $l8)
    if ($scene =~ LE07*) set exclude = 5
    if ($scene =~ LC08*) set exclude = 5

    foreach test ($tests)

        echo "Running test $test for ${scene}"

        # Link the test input product files
        ln -sf $ESPA_UNIT_TEST_DATA_DIR/espa-product-formatter/l*/${scene}* .

        if ("$test" == "01") set args=""
        if ("$test" == "02") set args="--sr_st_only"
        if ("$test" == "03") set args="--del_src_files"
        if ("$test" == "04") set args="--sr_st_only --del_src_files"

        set nexpected = `sh -c "ls ${scene}*TIF 2>>/dev/null" |wc -l`
        if ("$args" =~ *sr_st_only*) then
            @ nexpected = $nexpected - $exclude
        endif

        # Run the converter on the entire product
        echo "$bindir/convert_lpgs_to_espa --mtl=${scene}_MTL.txt"
        $bindir/convert_lpgs_to_espa --mtl=${scene}_MTL.txt $args
        if ($status != 0) then
            echo "Error running convert_lpgs_to_espa"
            exit 1
        endif

        # Compare the resulting XML to expected
        echo "Comparing ${scene}.XML to expected output"
        set ut="espa-product-formatter/convert_lpgs_to_espa"
        diff ${scene}.xml $ESPA_UNIT_TEST_DATA_DIR/$ut/${scene}.xml.$test
        if ($status != 0) then
            echo "Error in ${scene}.xml (compare to "
            echo -n "$ESPA_UNIT_TEST_DATA_DIR/$ut/${scene}.xml.$test)"
            exit 1
        endif

        # Make sure there are the correct number of output and
        # remaining input files
        echo "Looking for $nexpected output images"
        set ninput = `sh -c "ls ${scene}*TIF 2>>/dev/null" |wc -l`
        set nimg = `sh -c "ls ${scene}*img 2>>/dev/null" |wc -l`
        set nhdr = `sh -c "ls ${scene}*hdr 2>>/dev/null" |wc -l`

        if ("$nexpected" != "$nimg") then
            echo -n "Unexpected number of output image files ($nimg, "
            echo "expected $nexpected)"
            exit 1
        endif
        if ("$nimg" != "$nhdr") then
            echo "Unexpected number of output hdr files ($nhdr, expected $nimg)"
            exit 1
        endif
        if ("$args" =~ *del_src_files* && $ninput > 0) then
            echo "Unexpected number of input files remain ($ninput)"
            exit 1
        endif

        # Clean up links
        rm ${scene}*TIF ${scene}*MTL.xml ${scene}*txt
        rm -f README
        # Clean up output files
        rm ${scene}*img ${scene}*hdr ${scene}.xml

        echo "Completed test $test for ${scene}"
        echo ""
    end
end

echo ""

exit 0
