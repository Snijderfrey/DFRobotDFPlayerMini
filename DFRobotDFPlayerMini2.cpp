/*!
 * @file DFRobotDFPlayerMini2.cpp
 * @brief DFPlayer - An Arduino Mini MP3 Player From DFRobot
 * @n Header file for DFRobot's DFPlayer
 *
 * @copyright	[DFRobot]( http://www.dfrobot.com ), 2016
 * @copyright	GNU Lesser General Public License
 *
 * @author [Angelo](Angelo.qiao@dfrobot.com)
 * @version  V1.0.3
 * @date  2016-12-07
 */

#include "DFRobotDFPlayerMini2.h"

void DFRobotDFPlayerMini2::setTimeOut(unsigned long timeOutDuration){
  _timeOutDuration = timeOutDuration;
}

void DFRobotDFPlayerMini2::uint16ToArray(uint16_t value, uint8_t *array){
  *array = (uint8_t)(value>>8);
  *(array+1) = (uint8_t)(value);
}

uint16_t DFRobotDFPlayerMini2::calculateCheckSum(uint8_t *buffer){
  uint16_t sum = 0;
  for (int i=Stack_Version; i<Stack_CheckSum; i++) {
    sum += buffer[i];
  }
  return -sum;
}

void DFRobotDFPlayerMini2::sendStack(){
  if (_sending[Stack_ACK]) {  //if the ack mode is on wait until the last transmition
    while (_isSending) {
      delay(0);
      available();
    }
  }

#ifdef _DEBUG
  Serial.println();
  Serial.print(F("sending:"));
  for (int i=0; i<DFPLAYER_SEND_LENGTH; i++) {
    Serial.print(_sending[i],HEX);
    Serial.print(F(" "));
  }
  Serial.println();
#endif
  _serial->write(_sending, DFPLAYER_SEND_LENGTH);
  _timeOutTimer = millis();
  _isSending = _sending[Stack_ACK];
  
  if (!_sending[Stack_ACK]) { //if the ack mode is off wait 10 ms after one transmition.
    delay(10);
  }
}

void DFRobotDFPlayerMini2::sendStack(uint8_t command){
  sendStack(command, 0);
}

void DFRobotDFPlayerMini2::sendStack(uint8_t command, uint16_t argument){
  _sending[Stack_Command] = command;
  uint16ToArray(argument, _sending+Stack_Parameter);
  uint16ToArray(calculateCheckSum(_sending), _sending+Stack_CheckSum);
  sendStack();
}

void DFRobotDFPlayerMini2::sendStack(uint8_t command, uint8_t argumentHigh, uint8_t argumentLow){
  uint16_t buffer = argumentHigh;
  buffer <<= 8;
  sendStack(command, buffer | argumentLow);
}

void DFRobotDFPlayerMini2::enableACK(){
  _sending[Stack_ACK] = 0x01;
}

void DFRobotDFPlayerMini2::disableACK(){
  _sending[Stack_ACK] = 0x00;
}

bool DFRobotDFPlayerMini2::waitAvailable(unsigned long duration){
  unsigned long timer = millis();
  if (!duration) {
    duration = _timeOutDuration;
  }
  while (!available()){
    if (millis() - timer > duration) {
      return false;
    }
    delay(0);
  }
  return true;
}

bool DFRobotDFPlayerMini2::begin(Stream &stream, bool isACK, bool doReset){
  _serial = &stream;
  
  if (isACK) {
    enableACK();
  }
  else{
    disableACK();
  }
  
  if (doReset) {
    reset();
    waitAvailable(2000);
    delay(200);
  }
  else {
    // assume same state as with reset(): online
    _handleType = DFPlayerCardOnline;
  }

  pl_mode_curr_track = 1;
  pl_mode_curr_folder = 1;
  playlist_mode = false;
  pl_mode_pausing = false;
  
  return (readType() == DFPlayerCardOnline) || (readType() == DFPlayerUSBOnline) || !isACK;
}

uint8_t DFRobotDFPlayerMini2::readType(){
  _isAvailable = false;
  return _handleType;
}

uint16_t DFRobotDFPlayerMini2::read(){
  _isAvailable = false;
  return _handleParameter;
}

bool DFRobotDFPlayerMini2::handleMessage(uint8_t type, uint16_t parameter){
  _receivedIndex = 0;
  _handleType = type;
  _handleParameter = parameter;
  _isAvailable = true;
  return _isAvailable;
}

bool DFRobotDFPlayerMini2::handleError(uint8_t type, uint16_t parameter){
  handleMessage(type, parameter);
  _isSending = false;
  return false;
}

uint8_t DFRobotDFPlayerMini2::readCommand(){
  _isAvailable = false;
  return _handleCommand;
}

void DFRobotDFPlayerMini2::parseStack(){
  uint8_t handleCommand = *(_received + Stack_Command);
  if (handleCommand == 0x41) { //handle the 0x41 ack feedback as a spcecial case, in case the pollusion of _handleCommand, _handleParameter, and _handleType.
    _isSending = false;
    return;
  }
  
  _handleCommand = handleCommand;
  _handleParameter =  arrayToUint16(_received + Stack_Parameter);

  switch (_handleCommand) {
    case 0x3D:
      handleMessage(DFPlayerPlayFinished, _handleParameter);
      break;
    case 0x3F:
      if (_handleParameter & 0x01) {
        handleMessage(DFPlayerUSBOnline, _handleParameter);
      }
      else if (_handleParameter & 0x02) {
        handleMessage(DFPlayerCardOnline, _handleParameter);
      }
      else if (_handleParameter & 0x03) {
        handleMessage(DFPlayerCardUSBOnline, _handleParameter);
      }
      break;
    case 0x3A:
      if (_handleParameter & 0x01) {
        handleMessage(DFPlayerUSBInserted, _handleParameter);
      }
      else if (_handleParameter & 0x02) {
        handleMessage(DFPlayerCardInserted, _handleParameter);
      }
      break;
    case 0x3B:
      if (_handleParameter & 0x01) {
        handleMessage(DFPlayerUSBRemoved, _handleParameter);
      }
      else if (_handleParameter & 0x02) {
        handleMessage(DFPlayerCardRemoved, _handleParameter);
      }
      break;
    case 0x40:
      handleMessage(DFPlayerError, _handleParameter);
      break;
    case 0x3C:
    case 0x3E:
    case 0x42:
    case 0x43:
    case 0x44:
    case 0x45:
    case 0x46:
    case 0x47:
    case 0x48:
    case 0x49:
    case 0x4B:
    case 0x4C:
    case 0x4D:
    case 0x4E:
    case 0x4F:
      handleMessage(DFPlayerFeedBack, _handleParameter);
      break;
    default:
      handleError(WrongStack);
      break;
  }
}

uint16_t DFRobotDFPlayerMini2::arrayToUint16(uint8_t *array){
  uint16_t value = *array;
  value <<=8;
  value += *(array+1);
  return value;
}

bool DFRobotDFPlayerMini2::validateStack(){
  return calculateCheckSum(_received) == arrayToUint16(_received+Stack_CheckSum);
}

bool DFRobotDFPlayerMini2::available(){
  while (_serial->available()) {
    delay(0);
    if (_receivedIndex == 0) {
      _received[Stack_Header] = _serial->read();
#ifdef _DEBUG
      Serial.print(F("received:"));
      Serial.print(_received[_receivedIndex],HEX);
      Serial.print(F(" "));
#endif
      if (_received[Stack_Header] == 0x7E) {
        _receivedIndex ++;
      }
    }
    else{
      _received[_receivedIndex] = _serial->read();
#ifdef _DEBUG
      Serial.print(_received[_receivedIndex],HEX);
      Serial.print(F(" "));
#endif
      switch (_receivedIndex) {
        case Stack_Version:
          if (_received[_receivedIndex] != 0xFF) {
            return handleError(WrongStack);
          }
          break;
        case Stack_Length:
          if (_received[_receivedIndex] != 0x06) {
            return handleError(WrongStack);
          }
          break;
        case Stack_End:
#ifdef _DEBUG
          Serial.println();
#endif
          if (_received[_receivedIndex] != 0xEF) {
            return handleError(WrongStack);
          }
          else{
            if (validateStack()) {
              _receivedIndex = 0;
              parseStack();
              return _isAvailable;
            }
            else{
              return handleError(WrongStack);
            }
          }
          break;
        default:
          break;
      }
      _receivedIndex++;
    }
  }
  
  if (_isSending && (millis()-_timeOutTimer>=_timeOutDuration)) {
    return handleError(TimeOut);
  }
  
  return _isAvailable;
}

void DFRobotDFPlayerMini2::next(){
  sendStack(0x01);
}

void DFRobotDFPlayerMini2::previous(){
  sendStack(0x02);
}

void DFRobotDFPlayerMini2::play(int fileNumber){
  sendStack(0x03, fileNumber);
}

void DFRobotDFPlayerMini2::volumeUp(){
  sendStack(0x04);
}

void DFRobotDFPlayerMini2::volumeDown(){
  sendStack(0x05);
}

void DFRobotDFPlayerMini2::volume(uint8_t volume){
  sendStack(0x06, volume);
}

void DFRobotDFPlayerMini2::EQ(uint8_t eq) {
  sendStack(0x07, eq);
}

void DFRobotDFPlayerMini2::loop(int fileNumber) {
  sendStack(0x08, fileNumber);
}

void DFRobotDFPlayerMini2::outputDevice(uint8_t device) {
  sendStack(0x09, device);
  delay(200);
}

void DFRobotDFPlayerMini2::sleep(){
  sendStack(0x0A);
}

void DFRobotDFPlayerMini2::reset(){
  sendStack(0x0C);
}

void DFRobotDFPlayerMini2::start(){
  sendStack(0x0D);
}

void DFRobotDFPlayerMini2::pause(){
  sendStack(0x0E);
}

void DFRobotDFPlayerMini2::playFolder(uint8_t folderNumber, uint8_t fileNumber){
  sendStack(0x0F, folderNumber, fileNumber);
}

void DFRobotDFPlayerMini2::outputSetting(bool enable, uint8_t gain){
  sendStack(0x10, enable, gain);
}

void DFRobotDFPlayerMini2::enableLoopAll(){
  sendStack(0x11, 0x01);
}

void DFRobotDFPlayerMini2::disableLoopAll(){
  sendStack(0x11, 0x00);
}

void DFRobotDFPlayerMini2::playMp3Folder(int fileNumber){
  sendStack(0x12, fileNumber);
}

void DFRobotDFPlayerMini2::advertise(int fileNumber){
  bool prev = read_play_status_from_pin();
  bool curr;

  sendStack(0x13, fileNumber);

  if (playlist_mode) {
    byte transition_counter = 0;
    while (transition_counter < 4) {
      curr = read_play_status_from_pin();
      if (prev != curr) {
        transition_counter++;
        prev = curr;
      }
    }
  }
  /*unsigned long timex = millis();
  while (millis()-timex < 2000) {
    curr = read_play_status_from_pin();
    if (prev != curr) {
      Serial.print("Play status transistion: ");
      Serial.print(prev);
      Serial.print(" -> ");
      Serial.print(curr);
      Serial.print(" after ");
      Serial.print(millis()-timex);
      Serial.println(" ms");
      prev = curr;
    }
  }*/
}

void DFRobotDFPlayerMini2::playLargeFolder(uint8_t folderNumber, uint16_t fileNumber){
  sendStack(0x14, (((uint16_t)folderNumber) << 12) | fileNumber);
}

void DFRobotDFPlayerMini2::stopAdvertise(){
  sendStack(0x15);
}

void DFRobotDFPlayerMini2::stop(){
  sendStack(0x16);
}

void DFRobotDFPlayerMini2::loopFolder(int folderNumber){
  sendStack(0x17, folderNumber);
}

void DFRobotDFPlayerMini2::randomAll(){
  sendStack(0x18);
}

void DFRobotDFPlayerMini2::enableLoop(){
  sendStack(0x19, 0x00);
}

void DFRobotDFPlayerMini2::disableLoop(){
  sendStack(0x19, 0x01);
}

void DFRobotDFPlayerMini2::enableDAC(){
  sendStack(0x1A, 0x00);
}

void DFRobotDFPlayerMini2::disableDAC(){
  sendStack(0x1A, 0x01);
}

int DFRobotDFPlayerMini2::readState(){
  sendStack(0x42);
  if (waitAvailable()) {
    if (readType() == DFPlayerFeedBack) {
      return read();
    }
    else{
      return -1;
    }
  }
  else{
    return -1;
  }
}

int DFRobotDFPlayerMini2::readVolume(){
  sendStack(0x43);
  if (waitAvailable()) {
    return read();
  }
  else{
    return -1;
  }
}

int DFRobotDFPlayerMini2::readEQ(){
  sendStack(0x44);
  if (waitAvailable()) {
    if (readType() == DFPlayerFeedBack) {
      return read();
    }
    else{
      return -1;
    }
  }
  else{
    return -1;
  }
}

int DFRobotDFPlayerMini2::readFileCounts(uint8_t device){
  switch (device) {
    case DFPLAYER_DEVICE_U_DISK:
      sendStack(0x47);
      break;
    case DFPLAYER_DEVICE_SD:
      sendStack(0x48);
      break;
    case DFPLAYER_DEVICE_FLASH:
      sendStack(0x49);
      break;
    default:
      break;
  }
  
  if (waitAvailable()) {
    if (readType() == DFPlayerFeedBack) {
      return read();
    }
    else{
      return -1;
    }
  }
  else{
    return -1;
  }
}

int DFRobotDFPlayerMini2::readCurrentFileNumber(uint8_t device){
  switch (device) {
    case DFPLAYER_DEVICE_U_DISK:
      sendStack(0x4B);
      break;
    case DFPLAYER_DEVICE_SD:
      sendStack(0x4C);
      break;
    case DFPLAYER_DEVICE_FLASH:
      sendStack(0x4D);
      break;
    default:
      break;
  }
  if (waitAvailable()) {
    if (readType() == DFPlayerFeedBack) {
      return read();
    }
    else{
      return -1;
    }
  }
  else{
    return -1;
  }
}

int DFRobotDFPlayerMini2::readFileCountsInFolder(int folderNumber){
  sendStack(0x4E, folderNumber);
  if (waitAvailable()) {
    if (readType() == DFPlayerFeedBack) {
      return read();
    }
    else{
      return -1;
    }
  }
  else{
    return -1;
  }
}

int DFRobotDFPlayerMini2::readFolderCounts(){
  sendStack(0x4F);
  if (waitAvailable()) {
    if (readType() == DFPlayerFeedBack) {
      return read();
    }
    else{
      return -1;
    }
  }
  else{
    return -1;
  }
}

int DFRobotDFPlayerMini2::readFileCounts(){
  return readFileCounts(DFPLAYER_DEVICE_SD);
}

int DFRobotDFPlayerMini2::readCurrentFileNumber(){
  return readCurrentFileNumber(DFPLAYER_DEVICE_SD);
}


// The following methods were added to the original
// library for playlist mode.

void DFRobotDFPlayerMini2::pl_mode_play_track() {
  if (pl_mode_curr_track <= file_counts[pl_mode_curr_folder-1]) {
    playFolder(pl_mode_curr_folder, pl_mode_curr_track);
    wait_for_status_update(1);
#ifdef _DEBUG
    Serial.print("Playing track: ");
    Serial.println(pl_mode_curr_track);
#endif
    playlist_mode = true;
    pl_mode_pausing = false;
  } else {
    pl_mode_stop(false);
  }
}

void DFRobotDFPlayerMini2::pl_mode_stop(bool hard_stop) {
  playlist_mode = false;
  pl_mode_pausing = false;
  pl_mode_curr_track = 1;
  if (hard_stop) {
    stop();
    wait_for_status_update(0);
#ifdef _DEBUG
    Serial.println("Playing stopped.");
#endif
  }
}

void DFRobotDFPlayerMini2::get_file_counts() {
  for (byte ii=0; ii<MAX_PLAYLIST; ii++) {
    file_counts[ii] = readFileCountsInFolder(ii+1);
    if (file_counts[ii] == -1) {
      break;
    }
    
#ifdef _DEBUG
    Serial.print("Files in playlist ");
    Serial.print(ii+1);
    Serial.print(": ");
    Serial.println(file_counts[ii]);
#endif
  }
}

void DFRobotDFPlayerMini2::pl_mode_change_folder(byte playlist) {
  pl_mode_stop(false);
  pl_mode_curr_folder = playlist;
  
  playFolder(pl_mode_curr_folder, 1);
#ifdef _DEBUG
  Serial.print("Current playlist: ");
  Serial.println(pl_mode_curr_folder);
  Serial.print("File count ");
  Serial.println(file_counts[pl_mode_curr_folder-1]);
  /*Serial.print("Real time file count: ");
  Serial.println(readFileCountsInFolder(pl_mode_curr_folder));*/
#endif
}

void DFRobotDFPlayerMini2::pl_mode_next() {
  bool last_track;
  if (pl_mode_curr_track < file_counts[pl_mode_curr_folder-1]) {
    last_track = false;
    pl_mode_curr_track++;
  } else {
    last_track = true;
  }

  if(read_play_status_from_pin() == 1) {
    stop();
    wait_for_status_update(0);
  }
  
  if (playlist_mode && !last_track) {
    pl_mode_play_track();
  } else if (playlist_mode && last_track) {
    pl_mode_stop(false);
  } else if (!playlist_mode && !pl_mode_pausing && !last_track) {
    //playMp3Folder(105);
  }
}

void DFRobotDFPlayerMini2::pl_mode_previous() {
  bool first_track;
  if (pl_mode_curr_track>1) {
    first_track = false;
    pl_mode_curr_track--;
  } else {
    first_track = true;
  }

  if(read_play_status_from_pin() == 1) {
    stop();
    wait_for_status_update(0);
  }
  
  if (playlist_mode) {
    pl_mode_play_track();
  } else if (!playlist_mode && !pl_mode_pausing && !first_track) {
    //playMp3Folder(106);
  }
}

void DFRobotDFPlayerMini2::pl_mode_pause_resume() {
  if (playlist_mode && read_play_status_from_pin()) {
    playlist_mode = false;
    pl_mode_pausing = true;
    pause();
    wait_for_status_update(0);
#ifdef _DEBUG
    Serial.println("Playback paused.");
#endif
  } else if (!playlist_mode && !read_play_status_from_pin() && pl_mode_pausing) {
    playlist_mode = true;
    pl_mode_pausing = false;
    start();
    wait_for_status_update(1);
#ifdef _DEBUG
    Serial.println("Playback resumed.");
#endif
  }
}

bool DFRobotDFPlayerMini2::read_play_status_from_pin() {
  play_status = !digitalRead(PLAYING_PIN);
  return play_status;
}

bool DFRobotDFPlayerMini2::pl_mode_is_active() {
  return playlist_mode;
}

bool DFRobotDFPlayerMini2::pl_mode_check_playback() {
  if (playlist_mode && !read_play_status_from_pin()) {
    pl_mode_next();
    return true;
  } else {
    return false;
  }
}

byte DFRobotDFPlayerMini2::pl_mode_read_curr_track() {
  return pl_mode_curr_track;
}

unsigned long DFRobotDFPlayerMini2::wait_for_status_update(bool next_status) {
  unsigned long timer_start = millis();
  unsigned long status_update_delay;
  while (read_play_status_from_pin() != next_status && status_update_delay < 300) {
    //wait for play status update or timeout
    status_update_delay = millis() - timer_start;
  }
#ifdef _DEBUG
  bool timeout;
  if (read_play_status_from_pin() != next_status) {
    timeout = true;
  } else {
    timeout = false;
  }
  if (timeout) {
    Serial.println("Play status update not successful before timeout.");
  }
  Serial.print("Play status now ");
  Serial.print(read_play_status_from_pin());
  Serial.print(", delay: ");
  Serial.print(status_update_delay);
  Serial.print(" ms; (timeout ");
  Serial.print(timeout);
  Serial.println(")");
#endif

  return status_update_delay;
}
