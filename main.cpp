#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "Block.h"
#include "picosha2.h"
#include "rapidjson/document.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

using namespace std;
using namespace rapidjson;

class Blockchain {
private:
  bool success;
  string file;
  vector<int> difficultyList;
  unordered_map<string, vector<string>> senderMap;
  unordered_map<string, vector<string>> receiverMap;
  string chainHash;
  vector<unordered_map<string, string>> blockchain;
  unordered_map<string, string> blockData;
  unordered_map<string, string> block;
  Document document;

  bool loadJsonFile() {
    ifstream file_stream(file);
    stringstream buffers;
    buffers << file_stream.rdbuf();
    string file_contents = buffers.str();
    document.Parse(file_contents.c_str());
    if (!document.IsObject()) {
      return false;
    }

    if (document.HasMember("difficultyList")) {
      const Value &difficultyListVal = document["difficultyList"];
      if (difficultyListVal.IsArray()) {
        for (SizeType i = 0; i < difficultyListVal.Size(); i++) {
          if (difficultyListVal[i].IsInt()) {
            this->difficultyList.push_back(difficultyListVal[i].GetInt());
          }
        }
      }
    } else {
      return false;
    }

    if (document.HasMember("sender_map") && document["sender_map"].IsObject()) {
      const Value &senderMaps = document["sender_map"];

      for (auto j = senderMaps.MemberBegin(); j != senderMaps.MemberEnd();
           ++j) {
        const string &key = j->name.GetString();
        const Value &value = j->value.GetArray();

        if (value.IsArray()) {
          vector<string> receivers;
          for (SizeType i = 0; i < value.Size(); ++i) {
            const Value &receiver = value[i];
            if (receiver.IsString()) {
              receivers.push_back(receiver.GetString());
            }
          }
          this->senderMap[key] = receivers;
        }
      }
    } else {
      return false;
    }

    if (document.HasMember("receiver_map") &&
        document["receiver_map"].IsObject()) {
      const auto &receiverMaps = document["receiver_map"];

      for (auto it = receiverMaps.MemberBegin(); it != receiverMaps.MemberEnd();
           ++it) {
        const string &key = it->name.GetString();
        const Value &value = it->value.GetArray();

        if (value.IsArray()) {
          vector<string> receivers;
          for (SizeType i = 0; i < value.Size(); ++i) {
            const Value &receiver = value[i];
            if (receiver.IsString()) {
              receivers.push_back(receiver.GetString());
            }
          }
          this->receiverMap[key] = receivers;
        }
      }
    } else {
      return false;
    }

    if (document.HasMember("chainhash")) {
      this->chainHash = document["chainhash"].GetString();
    } else {
      return false;
    }

    if (document.HasMember("blockchain")) {
      const Value &blockchainVal = document["blockchain"];
      if (blockchainVal.IsArray()) {
        for (SizeType i = 0; i < blockchainVal.Size(); i++) {
          const Value &block = blockchainVal[i];

          unordered_map<string, string> blockMap;
          if (block.HasMember("previoushash") &&
              block["previoushash"].IsString()) {
            blockMap["previoushash"] = block["previoushash"].GetString();
          }
          if (block.HasMember("sender") && block["sender"].IsString()) {
            blockMap["sender"] = block["sender"].GetString();
          }
          if (block.HasMember("recipient") && block["recipient"].IsString()) {
            blockMap["recipient"] = block["recipient"].GetString();
          }
          if (block.HasMember("data") && block["data"].IsString()) {
            blockMap["data"] = block["data"].GetString();
          }
          if (block.HasMember("nonce") && block["nonce"].IsUint64()) {
            blockMap["nonce"] = to_string(block["nonce"].GetUint64());
          }
          if (block.HasMember("difficulty") && block["difficulty"].IsUint64()) {
            blockMap["difficulty"] =
                to_string(block["difficulty"].GetUint64());
          }
          this->blockchain.push_back(blockMap);
        }
      }
    } else {
      return false;
    }

    return true;
  }

  string hashBlock(unordered_map<string, string> block, string newNonce,
                    string newData) {
    string data = block["data"];
    string previousHash = block["previoushash"];
    string nonce = block["nonce"];
    string sender = block["sender"];
    string recipient = block["recipient"];

    string concatenated_string =
        (data + previousHash + nonce + sender + recipient);

    if (newNonce != "") {
      if (newData != "") {
        concatenated_string += newNonce;
        concatenated_string += newData;
      }
    }

    // hashing in picosha 256
    vector<unsigned char> hash(picosha2::k_digest_size);
    picosha2::hash256(concatenated_string.begin(), concatenated_string.end(),
                      hash.begin(), hash.end());

    return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
  }

  // Get Block At Index
  unordered_map<string, string> getBlock(int index) {
    if (index < 0 || index >= this->blockchain.size()) {
      throw out_of_range("Block index out of range");
    }
    return blockchain[index];
  }

// convert block to unordered map
  unordered_map<string, string> convertBlock(Block blocks) {
    unordered_map<string, string> block;

    block["previoushash"] = blocks.getPreviousHash();
    block["sender"] = blocks.getSender();
    block["recipient"] = blocks.getRecipient();
    block["data"] = blocks.getData();
    block["nonce"] = to_string(blocks.getNonce());
    block["difficulty"] = to_string(blocks.getDifficultyList());

    return block;
  }

  bool startsWithNZeros(string s, int n) {
    if (s.length() < n) {
      return false;
    }
    for (int i = 0; i < n; i++) {
      if (s[i] != '0') {
        return false;
      }
    }
    return true;
  }

  long long int linearSearch(int start, int difficulty,
                              unordered_map<string, string> block,
                              string newData) {
    int i = start;
    while (true) {
      string hex_str = hashBlock(block, to_string(i), newData);
      if (startsWithNZeros(hex_str, difficulty)) {
        return i;
      }
      i++;
    }

    return i;
  }

public:
  Blockchain() {
    // default block
    Block genesis = Block("", "", "", "Leeroy Jenkins", 0, 2);

    this->difficultyList.push_back(2);
    this->senderMap = {};
    this->receiverMap = {};
    this->chainHash = hashBlock(convertBlock(genesis), "", "");
    this->blockchain.push_back(convertBlock(genesis));
  }

  Blockchain(const string &file_) {
    FILE *files = fopen(file_.c_str(), "r");
    if (files) {
      fclose(files);

      file = file_;
      try {
        bool isEmpty = loadJsonFile();
        if (isEmpty == true) {
          this->success = true;
        } else {
          this->success = false;
        }
      } catch (const exception &ex) {
        this->success = false;
      }
    } else {
      this->success = false;
    }
  }

  bool getSuccess() { return this->success; }

// 1. add transaction
  void addTransaction() {
    string sender;
    string receiver;
    string data;
    unordered_map<string, string> newBlock;

    cin.clear();
    cin.ignore();

    cout << "Sender: ";
    getline(cin, sender);

    cout << "Receiver: ";
    getline(cin, receiver);

    cout << "Data: ";
    getline(cin, data);

    newBlock["sender"] = sender;
    newBlock["recipient"] = receiver;
    newBlock["data"] = data;
    newBlock["previoushash"] =
        hashBlock(getBlock(int(blockchain.size() - 1)), "", "");
    int difficulty = 2;
    newBlock["difficulty"] = to_string(difficulty);
    this->difficultyList.push_back(difficulty);
    newBlock["nonce"] = calculateNonce(
        difficulty, getBlock(int(blockchain.size() - 1)), data);
    this->blockchain.push_back(newBlock);

    if (this->senderMap.count(sender) == 0) {
      this->senderMap[sender] = vector<string>{receiver};
    } else {
      auto &receivers = this->senderMap[sender];
      if (find(receivers.begin(), receivers.end(), receiver) ==
          receivers.end()) {
        receivers.push_back(receiver);
      }
    }

    // Update receiver map
    if (this->receiverMap.count(receiver) == 0) {
      this->receiverMap[receiver] = vector<string>{sender};
    } else {
      auto &senders = this->receiverMap[receiver];
      if (find(senders.begin(), senders.end(), sender) == senders.end()) {
        senders.push_back(sender);
      }
    }
  }

// 2. Verify blockchain
  void verifyBlockchain() {
    if (this->difficultyList.size() != this->blockchain.size()) {
      cout << "Blockchain size is not the same as the difficulty list size"
           << endl;
      return;
    }
    for (int i = 1; i < int(this->blockchain.size()); i++) {
      unordered_map<string, string> tempCurrBlock = getBlock(i);
      unordered_map<string, string> tempPrevBlock= getBlock(i - 1);

      string prevBlockHash = hashBlock(tempPrevBlock, "", "");
      string currentNonceBlock =
          calculateNonce(this->difficultyList[i], tempPrevBlock,
                          tempCurrBlock["data"]);

      if (prevBlockHash == tempCurrBlock["previoushash"] &&
          currentNonceBlock == tempCurrBlock["nonce"]) {
        cout << "Block at index " << i << " is a valid block " << endl;
      } else {
        cout << "Block at index " << i << " is not a valid block "
             << endl;
      }
    }
  }

//3. view block

  void viewBlockchain() {
    for (auto &block : blockchain) {
      cout << "Previous Hash: " << block["previoushash"] << endl;
      cout << "Sender: " << block["sender"] << endl;
      cout << "Recipient: " << block["recipient"] << endl;
      cout << "Data: " << block["data"] << endl;
      cout << "Nonce: " << block["nonce"].c_str() << endl;
      cout << "Difficulty: " << block["difficulty"].c_str() << endl;
      cout << endl;
    }
  }

//4. corrupt block
  void corruptBlockchain() {
    int blockIndex;
    cout << "Enter block index number: ";
    cin >> blockIndex;

    while (cin.fail() || (blockIndex < 0 ||
                          blockIndex > int(this->blockchain.size() - 1))) {
      cin.clear();
      cin.ignore(numeric_limits<streamsize>::max(), '\n');
      cout << "Invalid index! Please try again: ";
      cin >> blockIndex;
    }

    unordered_map<string, string> block = getBlock(blockIndex);
    string data;
    cout << "Input a new data: ";
    cin >> data;
    block["data"] = data;
    this->blockchain[blockIndex] = block;
  }

// 5. fix corrupt
  void fixCorruption() {
    for (int i = 1; i < this->blockchain.size(); i++) {
      unordered_map<string, string> currentBlock = getBlock(i);
      unordered_map<string, string> prevBlock = getBlock(i - 1);

      string new_hash = hashBlock(prevBlock, "", "");
      currentBlock["previoushash"] = new_hash;

      int difficulty = difficultyList[i];
      string nonce_returned =
          calculateNonce(difficulty, prevBlock, currentBlock["data"]);
      currentBlock["nonce"] = nonce_returned;

      this->blockchain[i] = currentBlock;
    }
  }

//6. export file
  void exportJson() {
    string fileName;
    cout << "Enter name of file to save JSON to "
            "(C:\\Users\\<user>\\Downloads\\name.txt): ";
    cin >> fileName;

    Document doc;
    doc.SetObject();

    struct {
      const char *key;
      const char *value;
    } defaultVal[] = {{"difficulty_list", "[]"},
                          {"sender_map", "{}"},
                          {"receiver_map", "{}"},
                          {"chainhash", ""},
                          {"blockchain", "[]"}};

    for (auto &kv : defaultVal) {
      Value value;
      if (kv.key == string("difficulty_list")) {
        Value difficultyArray(kArrayType);
        for (const auto &diff : difficultyList) {
          difficultyArray.PushBack(Value().SetInt(diff), doc.GetAllocator());
        }
        value = difficultyArray;
      } else if (kv.key == string("sender_map")) {
        Value senderMapVal(kObjectType);
        for (auto &kv : senderMap) {
          Value receivers(kArrayType);
          for (auto &r : kv.second) {
            Value receiver;
            receiver.SetString(r.c_str(), r.size(), doc.GetAllocator());
            receivers.PushBack(receiver, doc.GetAllocator());
          }
          Value key;
          key.SetString(kv.first.c_str(), kv.first.size(), doc.GetAllocator());
          senderMapVal.AddMember(key, receivers, doc.GetAllocator());
        }
        value = senderMapVal;
      } else if (kv.key == string("receiver_map")) {
        Value receiverMapVal(kObjectType);
        for (auto &kv : receiverMap) {
          Value senders(kArrayType);
          for (auto &s : kv.second) {
            Value sender;
            sender.SetString(s.c_str(), s.size(), doc.GetAllocator());
            senders.PushBack(sender, doc.GetAllocator());
          }
          Value key;
          key.SetString(kv.first.c_str(), kv.first.size(), doc.GetAllocator());
          receiverMapVal.AddMember(key, senders, doc.GetAllocator());
        }
        value = receiverMapVal;
      } else if (kv.key == string("chainhash")) {
        value.SetString(chainHash.c_str(), chainHash.size(),
                        doc.GetAllocator());
      } else if (kv.key == string("blockchain")) {
        Value blockchainArray(kArrayType);
        for (const auto &block_kv : blockchain) {
          Value blockObj(kObjectType);
          for (const auto &field : block_kv) {
            blockObj.AddMember(
                Value().SetString(field.first.c_str(), field.first.size(),
                                  doc.GetAllocator()),
                Value().SetString(field.second.c_str(), field.second.size(),
                                  doc.GetAllocator()),
                doc.GetAllocator());
          }
          blockchainArray.PushBack(blockObj, doc.GetAllocator());
        }
        value = blockchainArray;
      }

      Value keyVal(kv.key, doc.GetAllocator());
      doc.AddMember(keyVal, value, doc.GetAllocator());
    }

    StringBuffer buffer;
    PrettyWriter<StringBuffer> writer(buffer);
    doc.Accept(writer);
    string json_str = buffer.GetString();

    ofstream outfile(fileName);
    if (outfile.is_open()) {
      outfile << json_str;
      outfile.close();
      cout << "JSON saved to file " << fileName << endl;
    } else {
      cerr << "Error: could not open file for writing to: " << fileName << endl;
    }
  }

// 7. change difficulty
  void changeDifficulty() {
    int blockIndex;
    cout << "Enter block index number: ";
    cin >> blockIndex;

    while (cin.fail() || (blockIndex < 0 ||
                          blockIndex > int(this->blockchain.size() - 1))) {
      cin.clear();
      cin.ignore(numeric_limits<streamsize>::max(), '\n');
      cout << "Invalid index! Please try again: ";
      cin >> blockIndex;
    }

    unordered_map<string, string> block = getBlock(blockIndex);
    int difficulty;
    cout << "Input a new difficulty: ";
    cin >> difficulty;
    while (cin.fail() || (difficulty < 0 || difficulty > 8)) {
      cin.clear();
      cin.ignore(numeric_limits<streamsize>::max(), '\n');
      cout << "Invalid difficulty! Please try again: ";
      cin >> difficulty;
    }
    block["difficulty"] = to_string(difficulty);
    this->blockchain[blockIndex] = block;
  }

// 8. print sender map
  void printSenderMap() {
    for (const auto &entry : senderMap) {
      cout << entry.first << ": ";
      for (const auto &receiver : entry.second) {
        cout << receiver << " ";
      }
      cout << endl;
    }
  }

// 9. print reciever map
  void printReceiverMap() {
    for (const auto &entry : receiverMap) {
      cout << entry.first << ": ";
      for (const auto &sender : entry.second) {
        cout << sender << " ";
      }
      cout << endl;
    }
  }
// 10. terminate program - will be in main

// to calculate nonce
  string calculateNonce(int difficulty, unordered_map<string, string> block,
                         string data) {
    int start = 0;
    auto nonce = linearSearch(start, difficulty, block, data);

    return to_string(nonce);
  }
};

int main() {
  int blockOption;
  bool validOption = false;
  Blockchain blockchainObj;

  
  while (!validOption) {
    // prints the block option
    cout << "1. Create a new blockchain" << endl;
    cout << "2. Import a JSON file (.txt)" << endl;
    cout << "3. Exit the program" << endl;
    cout << "Enter your choice: ";
    cin >> blockOption;

    switch (blockOption) {
    case 1:
      blockchainObj = Blockchain();
      validOption = true;
      break;
    case 2: {
      string fileName;
      cout << "Enter name of file to load JSON to:";
      cin >> fileName;

      blockchainObj = Blockchain(fileName);
      if (!blockchainObj.getSuccess()) {
        cout << "Unable to load json \n" << endl;
        validOption = false;
        break;
      } else {
        validOption = true;
        break;
      }
    }
    case 3:
      cout << "Exiting program..." << endl;
      exit(0);
    default:
      cin.clear();
      cin.ignore(numeric_limits<streamsize>::max(), '\n');
      cout << "Invalid option! Please enter a valid integer." << endl;
      break;
    }
  }

  int menu_choice;
  do {
    // prints the menu
    cout << "1. Add transaction" << endl;
    cout << "2. Verify blockchain" << endl;
    cout << "3. View blockchain" << endl;
    cout << "4. Corrupt block" << endl;
    cout << "5. Fix corruption" << endl;
    cout << "6. Export blockchain to json" << endl;
    cout << "7. Change difficulty" << endl;
    cout << "8. Print recipients by sender" << endl;
    cout << "9. Print senders by recipient" << endl;
    cout << "10.Quit" << endl;

    cout << "Enter your choice: ";
    cin >> menu_choice;

    // call each method for each menu
    switch (menu_choice) {
    case 1:
      blockchainObj.addTransaction();
      break;
    case 2:
      blockchainObj.verifyBlockchain();
      break;
    case 3:
      blockchainObj.viewBlockchain();
      break;
    case 4:
      blockchainObj.corruptBlockchain();
      break;
    case 5:
      blockchainObj.fixCorruption();
      break;
    case 6:
      blockchainObj.exportJson();
      break;
    case 7:
      blockchainObj.changeDifficulty();
      break;
    case 8:
      blockchainObj.printSenderMap();
      break;
    case 9:
      blockchainObj.printReceiverMap();
      break;
    case 10:
      cout << "\nExiting program..." << endl;
      exit(0);
      break;
    default:
      cin.clear(); // Clear the fail state
      cin.ignore(numeric_limits<streamsize>::max(),
                 '\n'); // Ignore invalid input up to the next newline character
      cout << "\nInvalid choice. Please try again." << endl;
      break;
    }
  } while (menu_choice != 10);
}