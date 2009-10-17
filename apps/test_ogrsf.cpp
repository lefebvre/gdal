/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Formal test harnass for OGRLayer implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogrsf_frmts.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

int     bReadOnly = FALSE;
int     bVerbose = TRUE;

static void Usage();
static void TestOGRLayer( OGRDataSource * poDS, OGRLayer * poLayer, int bIsSQLLayer );

/************************************************************************/
/*                                main()                                */
/************************************************************************/

int main( int nArgc, char ** papszArgv )

{
    const char  *pszDataSource = NULL;
    char** papszLayers = NULL;
    const char  *pszSQLStatement = NULL;
    
/* -------------------------------------------------------------------- */
/*      Register format(s).                                             */
/* -------------------------------------------------------------------- */
    OGRRegisterAll();
    
/* -------------------------------------------------------------------- */
/*      Processing command line arguments.                              */
/* -------------------------------------------------------------------- */
    for( int iArg = 1; iArg < nArgc; iArg++ )
    {
        if( EQUAL(papszArgv[iArg],"-ro") )
            bReadOnly = TRUE;
        else if( EQUAL(papszArgv[iArg],"-q") )
            bVerbose = FALSE;
        else if( EQUAL(papszArgv[iArg],"-sql") && iArg + 1 < nArgc)
            pszSQLStatement = papszArgv[++iArg];
        else if( papszArgv[iArg][0] == '-' )
        {
            Usage();
        }
        else if (pszDataSource == NULL)
            pszDataSource = papszArgv[iArg];
        else
            papszLayers = CSLAddString(papszLayers, papszArgv[iArg]);
    }

    if( pszDataSource == NULL )
        Usage();

/* -------------------------------------------------------------------- */
/*      Open data source.                                               */
/* -------------------------------------------------------------------- */
    OGRDataSource       *poDS;
    OGRSFDriver         *poDriver;

    poDS = OGRSFDriverRegistrar::Open( pszDataSource, !bReadOnly, &poDriver );
    if( poDS == NULL && !bReadOnly )
    {
        poDS = OGRSFDriverRegistrar::Open( pszDataSource, FALSE, &poDriver );
        if( poDS != NULL && bVerbose )
        {
            printf( "Had to open data source read-only.\n" );
            bReadOnly = TRUE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Report failure                                                  */
/* -------------------------------------------------------------------- */
    if( poDS == NULL )
    {
        OGRSFDriverRegistrar    *poR = OGRSFDriverRegistrar::GetRegistrar();
        
        printf( "FAILURE:\n"
                "Unable to open datasource `%s' with the following drivers.\n",
                pszDataSource );

        for( int iDriver = 0; iDriver < poR->GetDriverCount(); iDriver++ )
        {
            printf( "  -> %s\n", poR->GetDriver(iDriver)->GetName() );
        }

        exit( 1 );
    }

/* -------------------------------------------------------------------- */
/*      Some information messages.                                      */
/* -------------------------------------------------------------------- */
    if( bVerbose )
        printf( "INFO: Open of `%s' using driver `%s' successful.\n",
                pszDataSource, poDriver->GetName() );

    if( bVerbose && !EQUAL(pszDataSource,poDS->GetName()) )
    {
        printf( "INFO: Internal data source name `%s'\n"
                "      different from user name `%s'.\n",
                poDS->GetName(), pszDataSource );
    }
    
/* -------------------------------------------------------------------- */
/*      Process optionnal SQL request.                                  */
/* -------------------------------------------------------------------- */
    if (pszSQLStatement != NULL)
    {
        OGRLayer  *poResultSet = poDS->ExecuteSQL(pszSQLStatement, NULL, NULL);
        if (poResultSet == NULL)
            exit(1);
            
        printf( "INFO: Testing layer %s.\n",
                    poResultSet->GetLayerDefn()->GetName() );
        TestOGRLayer( poDS, poResultSet, TRUE );
        
        poDS->ReleaseResultSet(poResultSet);
    }
/* -------------------------------------------------------------------- */
/*      Process each data source layer.                                 */
/* -------------------------------------------------------------------- */
    else if (papszLayers == NULL)
    {
        for( int iLayer = 0; iLayer < poDS->GetLayerCount(); iLayer++ )
        {
            OGRLayer        *poLayer = poDS->GetLayer(iLayer);

            if( poLayer == NULL )
            {
                printf( "FAILURE: Couldn't fetch advertised layer %d!\n",
                        iLayer );
                exit( 1 );
            }

            printf( "INFO: Testing layer %s.\n",
                    poLayer->GetLayerDefn()->GetName() );
            TestOGRLayer( poDS, poLayer, FALSE );
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Or process layers specified by the user                         */
/* -------------------------------------------------------------------- */
        while (*papszLayers)
        {
            OGRLayer        *poLayer = poDS->GetLayerByName(*papszLayers);

            if( poLayer == NULL )
            {
                printf( "FAILURE: Couldn't fetch requested layer %s!\n",
                        *papszLayers );
                exit( 1 );
            }
            
            printf( "INFO: Testing layer %s.\n",
                    poLayer->GetLayerDefn()->GetName() );
            TestOGRLayer( poDS, poLayer, FALSE );
            
            papszLayers ++;
        }
    }

/* -------------------------------------------------------------------- */
/*      Close down.                                                     */
/* -------------------------------------------------------------------- */
    OGRDataSource::DestroyDataSource(poDS);

#ifdef DBMALLOC
    malloc_dump(1);
#endif
    
    return 0;
}

/************************************************************************/
/*                               Usage()                                */
/************************************************************************/

static void Usage()

{
    printf( "Usage: test_ogrsf [-ro] [-q] datasource_name [[layer1_name, layer2_name, ...] | [-sql statement]]\n" );
    exit( 1 );
}

/************************************************************************/
/*                      TestOGRLayerFeatureCount()                      */
/*                                                                      */
/*      Verify that the feature count matches the actual number of      */
/*      features returned during sequential reading.                    */
/************************************************************************/

static void TestOGRLayerFeatureCount( OGRDataSource* poDS, OGRLayer *poLayer, int bIsSQLLayer )

{
    int         nFC = 0, nClaimedFC = poLayer->GetFeatureCount();
    OGRFeature  *poFeature;
    OGRSpatialReference * poSRS = poLayer->GetSpatialRef();
    int         bWarnAboutSRS = FALSE;

    poLayer->ResetReading();

    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        nFC++;

        if( poFeature->GetGeometryRef() != NULL
            && poFeature->GetGeometryRef()->getSpatialReference() != poSRS
            && !bWarnAboutSRS )
        {
            char        *pszLayerSRSWKT, *pszFeatureSRSWKT;
            
            bWarnAboutSRS = TRUE;

            if( poSRS != NULL )
                poSRS->exportToWkt( &pszLayerSRSWKT );
            else
                pszLayerSRSWKT = CPLStrdup("(NULL)");

            if( poFeature->GetGeometryRef()->getSpatialReference() != NULL )
                poFeature->GetGeometryRef()->
                    getSpatialReference()->exportToWkt( &pszFeatureSRSWKT );
            else
                pszFeatureSRSWKT = CPLStrdup("(NULL)");
            
            printf( "ERROR: Feature SRS differs from layer SRS.\n"
                    "Feature SRS = %s\n"
                    "Layer SRS = %s\n",
                    pszFeatureSRSWKT, pszLayerSRSWKT );
            CPLFree( pszLayerSRSWKT );
            CPLFree( pszFeatureSRSWKT );
        }
        
        OGRFeature::DestroyFeature(poFeature);
    }

    if( nFC != nClaimedFC )
        printf( "ERROR: Claimed feature count %d doesn't match actual, %d.\n",
                nClaimedFC, nFC );
    else if( nFC != poLayer->GetFeatureCount() )
        printf( "ERROR: Feature count at end of layer %d differs "
                "from at start, %d.\n",
                nFC, poLayer->GetFeatureCount() );
    else if( bVerbose )
        printf( "INFO: Feature count verified.\n" );
        
    if (!bIsSQLLayer)
    {
        CPLString osSQL;
        osSQL.Printf("SELECT COUNT(*) FROM \"%s\"", poLayer->GetLayerDefn()->GetName());
        OGRLayer* poSQLLyr = poDS->ExecuteSQL(osSQL.c_str(), NULL, NULL);
        if (poSQLLyr)
        {
            OGRFeature* poFeatCount = poSQLLyr->GetNextFeature();
            if (nClaimedFC != poFeatCount->GetFieldAsInteger(0))
            {
                printf( "ERROR: Claimed feature count %d doesn't match '%s' one, %d.\n",
                        nClaimedFC, osSQL.c_str(), poFeatCount->GetFieldAsInteger(0) );
            }
            OGRFeature::DestroyFeature(poFeatCount);
            poDS->ReleaseResultSet(poSQLLyr);
        }
    }

    if( bVerbose && !bWarnAboutSRS )
    {
        printf("INFO: Feature/layer spatial ref. consistency verified.\n");
    }
}

/************************************************************************/
/*                       TestOGRLayerRandomRead()                       */
/*                                                                      */
/*      Read the first 5 features, and then try to use random           */
/*      reading to reread 2 and 5 and verify that this works OK.        */
/*      Don't attempt if there aren't at least 5 features.              */
/************************************************************************/

static void TestOGRLayerRandomRead( OGRLayer *poLayer )

{
    OGRFeature  *papoFeatures[5], *poFeature;
    int         iFeature;

    poLayer->SetSpatialFilter( NULL );
    
    if( poLayer->GetFeatureCount() < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only %d features on layer,"
                    "skipping random read test.\n",
                    poLayer->GetFeatureCount() );
        
        return;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = poLayer->GetNextFeature();
        CPLAssert( papoFeatures[iFeature] != NULL );
    }

/* -------------------------------------------------------------------- */
/*      Test feature 2.                                                 */
/* -------------------------------------------------------------------- */
    poFeature = poLayer->GetFeature( papoFeatures[1]->GetFID() );
    if( !poFeature->Equal( papoFeatures[1] ) )
    {
        printf( "ERROR: Attempt to randomly read feature %ld appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                papoFeatures[1]->GetFID() );

        return;
    }

    OGRFeature::DestroyFeature(poFeature);

/* -------------------------------------------------------------------- */
/*      Test feature 5.                                                 */
/* -------------------------------------------------------------------- */
    poFeature = poLayer->GetFeature( papoFeatures[4]->GetFID() );
    if( !poFeature->Equal( papoFeatures[4] ) )
    {
        printf( "ERROR: Attempt to randomly read feature %ld appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                papoFeatures[4]->GetFID() );

        return;
    }

    OGRFeature::DestroyFeature(poFeature);

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    for( iFeature = 0; iFeature < 5; iFeature++ )
        OGRFeature::DestroyFeature(papoFeatures[iFeature]);

    if( bVerbose )
        printf( "INFO: Random read test passed.\n" );
}


/************************************************************************/
/*                       TestOGRLayerSetNextByIndex()                   */
/*                                                                      */
/************************************************************************/

static void TestOGRLayerSetNextByIndex( OGRLayer *poLayer )

{
    OGRFeature  *papoFeatures[5], *poFeature;
    int         iFeature;

    poLayer->SetSpatialFilter( NULL );
    
    if( poLayer->GetFeatureCount() < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only %d features on layer,"
                    "skipping SetNextByIndex test.\n",
                    poLayer->GetFeatureCount() );
        
        return;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = poLayer->GetNextFeature();
        CPLAssert( papoFeatures[iFeature] != NULL );
    }

/* -------------------------------------------------------------------- */
/*      Test feature at index 1.                                        */
/* -------------------------------------------------------------------- */
    if (poLayer->SetNextByIndex(1) != OGRERR_NONE)
    {
        printf( "ERROR: SetNextByIndex(%d) failed.\n", 1 );
        return;
    }
    
    poFeature = poLayer->GetNextFeature();
    if( !poFeature->Equal( papoFeatures[1] ) )
    {
        printf( "ERROR: Attempt to read feature at index %d appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                1 );

        return;
    }

    OGRFeature::DestroyFeature(poFeature);
    
    poFeature = poLayer->GetNextFeature();
    if( !poFeature->Equal( papoFeatures[2] ) )
    {
        printf( "ERROR: Attempt to read feature after feature at index %d appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                1 );

        return;
    }

    OGRFeature::DestroyFeature(poFeature);
    
/* -------------------------------------------------------------------- */
/*      Test feature at index 3.                                        */
/* -------------------------------------------------------------------- */
    if (poLayer->SetNextByIndex(3) != OGRERR_NONE)
    {
        printf( "ERROR: SetNextByIndex(%d) failed.\n", 3 );
        return;
    }
    
    poFeature = poLayer->GetNextFeature();
    if( !poFeature->Equal( papoFeatures[3] ) )
    {
        printf( "ERROR: Attempt to read feature at index %d appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                3 );

        return;
    }

    OGRFeature::DestroyFeature(poFeature);
    
    poFeature = poLayer->GetNextFeature();
    if( !poFeature->Equal( papoFeatures[4] ) )
    {
        printf( "ERROR: Attempt to read feature after feature at index %d appears to\n"
                "       have returned a different feature than sequential\n"
                "       reading indicates should have happened.\n",
                3 );

        return;
    }

    OGRFeature::DestroyFeature(poFeature);

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    for( iFeature = 0; iFeature < 5; iFeature++ )
        OGRFeature::DestroyFeature(papoFeatures[iFeature]);

    if( bVerbose )
        printf( "INFO: SetNextByIndex() read test passed.\n" );
}

/************************************************************************/
/*                      TestOGRLayerRandomWrite()                       */
/*                                                                      */
/*      Test random writing by trying to switch the 2nd and 5th         */
/*      features.                                                       */
/************************************************************************/

static void TestOGRLayerRandomWrite( OGRLayer *poLayer )

{
    OGRFeature  *papoFeatures[5], *poFeature;
    int         iFeature;
    long        nFID2, nFID5;

    poLayer->SetSpatialFilter( NULL );
    
    if( poLayer->GetFeatureCount() < 5 )
    {
        if( bVerbose )
            printf( "INFO: Only %d features on layer,"
                    "skipping random write test.\n",
                    poLayer->GetFeatureCount() );
        
        return;
    }

    if( !poLayer->TestCapability( OLCRandomRead ) )
    {
        if( bVerbose )
            printf( "INFO: Skipping random write test since this layer "
                    "doesn't support random read.\n" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Fetch five features.                                            */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    
    for( iFeature = 0; iFeature < 5; iFeature++ )
    {
        papoFeatures[iFeature] = poLayer->GetNextFeature();
        CPLAssert( papoFeatures[iFeature] != NULL );
    }

/* -------------------------------------------------------------------- */
/*      Switch feature ids of feature 2 and 5.                          */
/* -------------------------------------------------------------------- */
    nFID2 = papoFeatures[1]->GetFID();
    nFID5 = papoFeatures[4]->GetFID();

    papoFeatures[1]->SetFID( nFID5 );
    papoFeatures[4]->SetFID( nFID2 );

/* -------------------------------------------------------------------- */
/*      Rewrite them.                                                   */
/* -------------------------------------------------------------------- */
    if( poLayer->SetFeature( papoFeatures[1] ) != OGRERR_NONE )
    {
        printf( "ERROR: Attempt to SetFeature(1) failed.\n" );
        return;
    }
    if( poLayer->SetFeature( papoFeatures[4] ) != OGRERR_NONE )
    {
        printf( "ERROR: Attempt to SetFeature(4) failed.\n" );
        return;
    }

/* -------------------------------------------------------------------- */
/*      Now re-read feature 2 to verify the effect stuck.               */
/* -------------------------------------------------------------------- */
    poFeature = poLayer->GetFeature( nFID5 );
    if( !poFeature->Equal(papoFeatures[1]) )
    {
        printf( "ERROR: Written feature didn't seem to retain value.\n" );
    }
    else
    {
        printf( "INFO: Random write test passed.\n" );
    }

/* -------------------------------------------------------------------- */
/*      Cleanup.                                                        */
/* -------------------------------------------------------------------- */
    OGRFeature::DestroyFeature(poFeature);

    for( iFeature = 0; iFeature < 5; iFeature++ )
        OGRFeature::DestroyFeature(papoFeatures[iFeature]);
}

/************************************************************************/
/*                         TestSpatialFilter()                          */
/*                                                                      */
/*      This is intended to be a simple test of the spatial             */
/*      filting.  We read the first feature.  Then construct a          */
/*      spatial filter geometry which includes it, install and          */
/*      verify that we get the feature.  Next install a spatial         */
/*      filter that doesn't include this feature, and test again.       */
/************************************************************************/

static void TestSpatialFilter( OGRLayer *poLayer )

{
    OGRFeature  *poFeature, *poTargetFeature;
    OGRPolygon  oInclusiveFilter, oExclusiveFilter;
    OGRLinearRing oRing;
    OGREnvelope sEnvelope;
    int         nInclusiveCount;

/* -------------------------------------------------------------------- */
/*      Read the target feature.                                        */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();
    poTargetFeature = poLayer->GetNextFeature();

    if( poTargetFeature == NULL )
    {
        printf( "INFO: Skipping Spatial Filter test for %s.\n"
                "      No features in layer.\n",
                poLayer->GetLayerDefn()->GetName() );
        return;
    }

    if( poTargetFeature->GetGeometryRef() == NULL )
    {
        printf( "INFO: Skipping Spatial Filter test for %s,\n"
                "      target feature has no geometry.\n",
                poTargetFeature->GetDefnRef()->GetName() );
        OGRFeature::DestroyFeature(poTargetFeature);
        return;
    }

    poTargetFeature->GetGeometryRef()->getEnvelope( &sEnvelope );

/* -------------------------------------------------------------------- */
/*      Construct inclusive filter.                                     */
/* -------------------------------------------------------------------- */
    
    oRing.setPoint( 0, sEnvelope.MinX - 10.0, sEnvelope.MinY - 10.0 );
    oRing.setPoint( 1, sEnvelope.MinX - 10.0, sEnvelope.MaxY + 10.0 );
    oRing.setPoint( 2, sEnvelope.MaxX + 10.0, sEnvelope.MaxY + 10.0 );
    oRing.setPoint( 3, sEnvelope.MaxX + 10.0, sEnvelope.MinY - 10.0 );
    oRing.setPoint( 4, sEnvelope.MinX - 10.0, sEnvelope.MinY - 10.0 );
    
    oInclusiveFilter.addRing( &oRing );

    poLayer->SetSpatialFilter( &oInclusiveFilter );

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();

    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->Equal(poTargetFeature) )
        {
            OGRFeature::DestroyFeature(poFeature);
            break;
        }
        else
            OGRFeature::DestroyFeature(poFeature);
    }

    if( poFeature == NULL )
    {
        printf( "ERROR: Spatial filter eliminated a feature unexpectedly!\n");
    }
    else if( bVerbose )
    {
        printf( "INFO: Spatial filter inclusion seems to work.\n" );
    }

    nInclusiveCount = poLayer->GetFeatureCount();

/* -------------------------------------------------------------------- */
/*      Construct exclusive filter.                                     */
/* -------------------------------------------------------------------- */
    oRing.setPoint( 0, sEnvelope.MinX - 20.0, sEnvelope.MinY - 20.0 );
    oRing.setPoint( 1, sEnvelope.MinX - 10.0, sEnvelope.MinY - 20.0 );
    oRing.setPoint( 2, sEnvelope.MinX - 10.0, sEnvelope.MinY - 10.0 );
    oRing.setPoint( 3, sEnvelope.MinX - 20.0, sEnvelope.MinY - 10.0 );
    oRing.setPoint( 4, sEnvelope.MinX - 20.0, sEnvelope.MinY - 20.0 );
    
    oExclusiveFilter.addRing( &oRing );

    poLayer->SetSpatialFilter( &oExclusiveFilter );

/* -------------------------------------------------------------------- */
/*      Verify that we can find the target feature.                     */
/* -------------------------------------------------------------------- */
    poLayer->ResetReading();

    while( (poFeature = poLayer->GetNextFeature()) != NULL )
    {
        if( poFeature->Equal(poTargetFeature) )
        {
            OGRFeature::DestroyFeature(poFeature);
            break;
        }
        else
            OGRFeature::DestroyFeature(poFeature);
    }

    if( poFeature != NULL )
    {
        printf( "ERROR: Spatial filter failed to eliminate"
                "a feature unexpectedly!\n");
    }
    else if( poLayer->GetFeatureCount() >= nInclusiveCount )
    {
        printf( "ERROR: GetFeatureCount() may not be taking spatial "
                "filter into account.\n" );
    }
    else if( bVerbose )
    {
        printf( "INFO: Spatial filter exclusion seems to work.\n" );
    }

    OGRFeature::DestroyFeature(poTargetFeature);
}


/************************************************************************/
/*                            TestOGRLayer()                            */
/************************************************************************/

static void TestOGRLayer( OGRDataSource* poDS, OGRLayer * poLayer, int bIsSQLLayer )

{
/* -------------------------------------------------------------------- */
/*      Verify that there is no spatial filter in place by default.     */
/* -------------------------------------------------------------------- */
    if( poLayer->GetSpatialFilter() != NULL )
    {
        printf( "WARN: Spatial filter in place by default on layer %s.\n",
                poLayer->GetLayerDefn()->GetName() );
        poLayer->SetSpatialFilter( NULL );
    }

/* -------------------------------------------------------------------- */
/*      Test feature count accuracy.                                    */
/* -------------------------------------------------------------------- */
    TestOGRLayerFeatureCount( poDS, poLayer, bIsSQLLayer );

/* -------------------------------------------------------------------- */
/*      Test spatial filtering                                          */
/* -------------------------------------------------------------------- */
    TestSpatialFilter( poLayer );

/* -------------------------------------------------------------------- */
/*      Test random reading.                                            */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCRandomRead ) )
    {
        TestOGRLayerRandomRead( poLayer );
    }
    
/* -------------------------------------------------------------------- */
/*      Test SetNextByIndex.                                            */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCFastSetNextByIndex ) )
    {
        TestOGRLayerSetNextByIndex( poLayer );
    }
    
/* -------------------------------------------------------------------- */
/*      Test random writing.                                            */
/* -------------------------------------------------------------------- */
    if( poLayer->TestCapability( OLCRandomWrite ) )
    {
        TestOGRLayerRandomWrite( poLayer );
    }
}
