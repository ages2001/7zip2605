/* Native command integration for Deflate64; input archives are read-only.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifdef __WATCOMC__
#pragma code_seg("D64_TEST_TEXT","D64_TEST_CODE")
static unsigned deflate64TestCs(void);
#pragma aux deflate64TestCs = "mov ax,cs" value [ax] modify exact [ax];
#endif
static int deflate64SelfTest(FILE *log){
 int valid=1,ok;unsigned i,baseline;struct stat info;
#ifdef __WATCOMC__
 unsigned segment=deflate64TestCs();
 if(!LockSegment(segment)){fputs("FAIL cannot retain Deflate64 test driver\n",log);return 0;}
#endif
 extractTesting=TRUE;extractFlat=FALSE;strcpy(extractDestination,"C:\\D64OUT");
 for(i=0;i<2;i++){
  SetWindowText(pathEdit,i?"C:\\DYN64.ZIP":"C:\\MAX64.ZIP");loadArchive();ok=opened&&lastResult==SZ_OK&&archive.NumFiles==1;
  if(ok){baseline=windowAllocationCount;performTest();ok=lastResult==SZ_OK&&windowAllocationCount==baseline;}
  if(ok){performExtract();ok=lastResult==SZ_OK&&windowAllocationCount==baseline&&verifyExtractedFile("C:\\D64OUT\\DATA.BIN",i?262144UL:262150UL,i?0xc790bff6UL:0xabcc482dUL);}
  fprintf(log,"%s Deflate64 %s: integrity, exact extraction and unchanged allocation count; result=%d\n",ok?"PASS":"FAIL",i?"dynamic trees":"65536 distance / 65538 length",lastResult);fflush(log);valid&=ok;
  if(!stat("C:\\D64OUT\\DATA.BIN",&info)&&remove("C:\\D64OUT\\DATA.BIN"))valid=0;
  if(!stat(extractDestination,&info)&&rmdir(extractDestination))valid=0;
  if(opened){dictionaryCancelAfter=20000;performExtract();ok=lastResult==SZ_ERROR_PROGRESS&&stat(extractDestination,&info)!=0&&windowAllocationCount==baseline;dictionaryCancelAfter=0;
   fprintf(log,"%s Deflate64 cancelled extraction removes partial output; result=%d\n",ok?"PASS":"FAIL",lastResult);valid&=ok;}
  closeArchive();valid&=!windowAllocationCount;
 }
 SetWindowText(pathEdit,"C:\\BAD64.ZIP");loadArchive();ok=opened;
 if(ok){performExtract();ok=lastResult==SZ_ERROR_CRC&&stat(extractDestination,&info)!=0;}
 fprintf(log,"%s Deflate64 corrupt CRC rejected before output; result=%d\n",ok?"PASS":"FAIL",lastResult);valid&=ok;closeArchive();
 /* A baseline 32 KiB Deflate archive still opens and verifies after both modes. */
 SetWindowText(pathEdit,"C:\\WIN16.ZIP");loadArchive();ok=opened;if(ok){performTest();ok=lastResult==SZ_OK;}
 fprintf(log,"%s ordinary Deflate remains usable after Deflate64\n",ok?"PASS":"FAIL");valid&=ok;closeArchive();
 createTesting=TRUE;createZipMode=TRUE;createMethod=2;
 strcpy(createDestination,"C:\\D84NEW.ZIP");ok=setCreateSource("C:\\C31IN.TXT");
 if(ok){SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);ok=lastResult==SZ_OK&&opened&&createMethod==2&&createZipMode&&archive.NumFiles==1;}
 fprintf(log,"%s GUI Deflate64 selection and verified ZIP creation; result=%d\n",ok?"PASS":"FAIL",lastResult);valid&=ok;
 if(ok){performExtract();ok=lastResult==SZ_OK&&verifyExtractedFile("C:\\D64OUT\\C31IN.TXT",102400UL,0x9a0e0c8cUL);}
 fprintf(log,"%s GUI-created Deflate64 exact extraction\n",ok?"PASS":"FAIL");valid&=ok;
 if(!stat("C:\\D64OUT\\C31IN.TXT",&info)&&remove("C:\\D64OUT\\C31IN.TXT"))valid=0;
 if(!stat(extractDestination,&info)&&rmdir(extractDestination))valid=0;
 SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);ok=lastResult==SZ_ERROR_WRITE;
 fprintf(log,"%s existing Deflate64 ZIP is protected\n",ok?"PASS":"FAIL");valid&=ok;
 strcpy(createDestination,"C:\\D84CANC.ZIP");createCancelAfterRead=TRUE;SendMessage(mainWindow,WM_COMMAND,ID_CREATE,0);createCancelAfterRead=FALSE;
 ok=lastResult==SZ_ERROR_PROGRESS&&stat(createDestination,&info)!=0;fprintf(log,"%s cancelled Deflate64 creation removes partial archive\n",ok?"PASS":"FAIL");valid&=ok;
 SetWindowText(pathEdit,"C:\\D84NEW.ZIP");loadArchive();ok=opened;
 if(ok){createMethod=2;SendMessage(mainWindow,WM_COMMAND,ID_UPDATE,0);ok=lastResult==SZ_OK&&opened&&createMethod==2&&archive.NumFiles==1;}
 fprintf(log,"%s GUI Deflate64 Update dialog and verified replacement; result=%d\n",ok?"PASS":"FAIL",lastResult);valid&=ok;
 createTesting=createZipMode=FALSE;createMethod=1;closeArchive();
 extractTesting=FALSE;valid&=!windowAllocationCount;fprintf(log,"%s Deflate64 driver releases all archive/cache blocks\n",!windowAllocationCount?"PASS":"FAIL");
#ifdef __WATCOMC__
 UnlockSegment(segment);
#endif
 return valid;
}
#ifdef __WATCOMC__
#pragma code_seg("GUI_TEST_TEXT")
#endif
