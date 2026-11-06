/* Guest integration tests for actual list selections and archive commands.
   SPDX-License-Identifier: LGPL-2.1-or-later */
#ifdef __WATCOMC__
#pragma code_seg("SELECT_TEST_TEXT")
#endif
static void selectionCheck(FILE *log,int *valid,int pass,const char *label){
 fprintf(log,"%s %s; result=%d blocks=%u\n",pass?"PASS":"FAIL",label,lastResult,windowAllocationCount);fflush(log);*valid&=pass;
}
static long selectionRow(const char *name){
 UInt32 i;for(i=0;i<archive.NumFiles;i++)if(renameMatch(archive.FileNames+2*archive.FileNameOffsets[i],name,0))return (long)i;return -1;
}
static int selectionPick(const char *first,const char *second,const char *third){
 const char *names[3];unsigned n;long row;names[0]=first;names[1]=second;names[2]=third;selectListItem(-1);
 for(n=0;n<3;n++)if(names[n]){row=selectionRow(names[n]);if(row<0)return 0;SendMessage(fileList,LB_SETSEL,TRUE,row);}
 return 1;
}
static int selectionSelfTestBody(FILE *log){
 unsigned which,baseline;int valid=1,ok;char path[260];struct stat info;UInt32 beforeCrc,beforeSize;FILE *src,*dst;Byte buffer[512];size_t n;
 extractTesting=TRUE;extractFlat=FALSE;strcpy(extractDestination,"C:\\S80OUT");
 for(which=0;which<2;which++){
  sprintf(path,"C:\\SEL80.%s",which?"ZIP":"7Z");SetWindowText(pathEdit,path);loadArchive();
  if(!opened||lastResult!=SZ_OK){selectionCheck(log,&valid,0,"selection fixture opens");break;}
  baseline=windowAllocationCount;ok=selectionPick("A/FIRST.BIN","ZERO.TXT","EMPTY");performExtract();
  ok=ok&&lastResult==SZ_OK&&windowAllocationCount==baseline&&verifyExtractedFile("C:\\S80OUT\\A\\FIRST.BIN",12291UL,0xba8ed439UL)&&verifyExtractedFile("C:\\S80OUT\\ZERO.TXT",0,0)&&!stat("C:\\S80OUT\\EMPTY",&info)&&stat("C:\\S80OUT\\B",&info)!=0;
  selectionCheck(log,&valid,ok,which?"ZIP selected files and empty directory; unselected unsafe name excluded":"solid 7z selected files and empty directory; unselected unsafe name excluded");
  performExtract();selectionCheck(log,&valid,lastResult==SZ_ERROR_WRITE&&verifyExtractedFile("C:\\S80OUT\\A\\FIRST.BIN",12291UL,0xba8ed439UL),"existing selected destination is preserved");
  if(remove("C:\\S80OUT\\A\\FIRST.BIN")||remove("C:\\S80OUT\\ZERO.TXT")||rmdir("C:\\S80OUT\\A")||rmdir("C:\\S80OUT\\EMPTY")||rmdir("C:\\S80OUT"))valid=0;
  ok=selectionPick("LONG-NAME-IGNORE.TXT",NULL,NULL);performExtract();selectionCheck(log,&valid,ok&&lastResult==SZ_ERROR_UNSUPPORTED&&stat(extractDestination,&info)!=0,"selected unrepresentable path rejected before output");
  ok=selectionPick("A/FIRST.BIN",NULL,NULL);windowAllocationFailureSize=(unsigned)(archive.NumFiles/8+(archive.NumFiles%8!=0));windowAllocationFailures=0;windowAllocationFailuresRemaining=1;
  performExtract();selectionCheck(log,&valid,ok&&lastResult==SZ_ERROR_MEM&&windowAllocationFailures==1&&windowAllocationCount==baseline&&stat(extractDestination,&info)!=0,"selection allocation failure leaves no output or leaked blocks");windowAllocationFailuresRemaining=windowAllocationFailureSize=windowAllocationFailures=0;
  extractCancelTesting=TRUE;performExtract();extractCancelTesting=FALSE;selectionCheck(log,&valid,lastResult==SZ_ERROR_PROGRESS&&windowAllocationCount==baseline&&stat(extractDestination,&info)!=0,"cancelled selected extraction leaves no output");
  extractFlat=TRUE;performExtract();selectionCheck(log,&valid,lastResult==SZ_OK&&verifyExtractedFile("C:\\S80OUT\\FIRST.BIN",12291UL,0xba8ed439UL)&&stat("C:\\S80OUT\\A",&info)!=0,"selected extraction without folder paths");if(remove("C:\\S80OUT\\FIRST.BIN")||rmdir("C:\\S80OUT"))valid=0;
  ok=selectionPick("A/Clash.txt","B/CLASH.TXT",NULL);performExtract();selectionCheck(log,&valid,ok&&lastResult==SZ_ERROR_UNSUPPORTED&&stat(extractDestination,&info)!=0,"selected flattened name collision rejected before output");extractFlat=FALSE;
  if(!which){ok=selectionPick("A",NULL,NULL);performExtract();selectionCheck(log,&valid,ok&&lastResult==SZ_OK&&verifyExtractedFile("C:\\S80OUT\\A\\FIRST.BIN",12291UL,0xba8ed439UL)&&verifyExtractedFile("C:\\S80OUT\\A\\CLASH.TXT",5,0x9271ee57UL)&&stat("C:\\S80OUT\\B",&info)!=0,"selected folder includes its descendants only");if(remove("C:\\S80OUT\\A\\FIRST.BIN")||remove("C:\\S80OUT\\A\\CLASH.TXT")||rmdir("C:\\S80OUT\\A")||rmdir("C:\\S80OUT"))valid=0;}
  selectListItem(-1);performExtract();selectionCheck(log,&valid,lastResult==SZ_ERROR_UNSUPPORTED&&stat(extractDestination,&info)!=0,"no selection retains whole-archive validation");closeArchive();selectionCheck(log,&valid,!windowAllocationCount,"selection operation buffers released");
 }
 SetWindowText(pathEdit,"C:\\CRC80.ZIP");loadArchive();ok=opened&&selectionPick("GOOD.TXT",NULL,NULL);performExtract();selectionCheck(log,&valid,ok&&lastResult==SZ_ERROR_CRC&&stat(extractDestination,&info)!=0,"unselected corrupt member still fails archive integrity");closeArchive();
 extractTesting=FALSE;deleteTesting=TRUE;
 for(which=0;which<2;which++){
  sprintf(path,"C:\\SG80.%s",which?"ZIP":"7Z");src=fopen(path,"rb");sprintf(path,"C:\\GD80.%s",which?"ZIP":"7Z");
  if(!src||!stat(path,&info)){if(src)fclose(src);selectionCheck(log,&valid,0,"delete fixture must be a new private copy");break;}
  dst=fopen(path,"wb");if(!dst){fclose(src);valid=0;break;}ok=1;
  while((n=fread(buffer,1,sizeof(buffer),src))!=0)if(fwrite(buffer,1,n,dst)!=n){ok=0;break;}
  ok&=!ferror(src);ok&=fclose(dst)==0;fclose(src);ok&=renameIdentity(path,&beforeCrc,&beforeSize);
  SetWindowText(pathEdit,path);loadArchive();ok=ok&&opened&&selectionPick("A","ZERO.TXT",NULL);baseline=windowAllocationCount;
  deleteTestChoice=IDNO;performDelete();selectionCheck(log,&valid,ok&&lastResult==SZ_ERROR_PROGRESS&&windowAllocationCount==baseline&&verifyExtractedFile(path,beforeSize,beforeCrc),"declined multiple delete preserves exact archive");deleteTestChoice=IDYES;
  windowAllocationFailureSize=2*sizeof(char *);windowAllocationFailures=0;windowAllocationFailuresRemaining=1;performDelete();selectionCheck(log,&valid,lastResult==SZ_ERROR_MEM&&windowAllocationFailures==1&&windowAllocationCount==baseline&&verifyExtractedFile(path,beforeSize,beforeCrc),"delete snapshot allocation failure preserves exact archive");windowAllocationFailuresRemaining=windowAllocationFailureSize=windowAllocationFailures=0;
  performDelete();ok=lastResult==SZ_OK&&opened&&selectionRow("A")<0&&selectionRow("A/FIRST.BIN")<0&&selectionRow("A/Clash.txt")<0&&selectionRow("ZERO.TXT")<0&&selectionRow("AB/KEEP.TXT")>=0&&selectionRow("B/SECOND.BIN")>=0;
  if(ok){performTest();ok=lastResult==SZ_OK;}selectionCheck(log,&valid,ok,which?"ZIP multiple delete removes selected subtree and file; sibling prefix preserved":"7z multiple delete removes selected subtree and file; sibling prefix preserved");closeArchive();selectionCheck(log,&valid,!windowAllocationCount,"delete snapshots released before reload and after close");
 }
 deleteTesting=FALSE;deleteTestChoice=IDYES;extractFlat=extractTesting=FALSE;return valid;
}
static int selectionSelfTest(FILE *log){return selectionSelfTestBody(log);}
#ifdef __WATCOMC__
#pragma code_seg()
#endif
