import sqlite3
import subprocess
import re
from PhoneticTranslation import PhoneticTranslation


class UpdateDic:
    numbers_to_words={
        1:"întâi",
        2:"doi",
        3:"trei",
        4:"patru",
        5:"cinci",
        6:"șase",
        7:"șapte",
        8:"opt",
        9:"nouă",
        10:"zece",
        11:"unsprezece",
        12:"doispreze",
        13:"treisprezece",
        14:"patrusprezece",
        15:"cincisprezece",
        16:"șasesprezece",
        17:"șaptesprezece",
        18:"optsprezece",
        19:"nouăsprezece",
        20:"douăzeci",
        21:"douăzecișiunu",
        22:"douăzecișidoi",
        23:"douăzecișitrei",
        24:"douăzecișipatru",
        2023:"douămiidouăzecișitrei",
        2024:"douămiidouăzecișipatru",
        2025:"douămiidouăzecișicinci",
        2026:"douămiidouăzecișișase",
        2027:"douămiidouăzecișișapte",
        2028:"douămiidouăzecișiopt",
        2022:"douămiidouăzecișidoi",
        2021:"douămiidouăzecișiunu",
        2020:"douămiidouăzeci"

    }
    def __init__(self,dic_file,db_file):
        self.dic_file=dic_file
        self.db_file=db_file
        with open(dic_file, "r", encoding="utf-8") as f:
            self.existing_words = {line.split(" ")[0] for line in f}

    def clean_word(self,word):
        w=word.lower().strip()
        w=re.sub(r"[^a-zăîșțâ0-9]","",w)
        if w.isdigit():
            return self.numbers_to_words.get(int(w),w)
        if not w:
            return None
        return w


#get the words from the database
    def get_words_from_db(self,conn):
        """Extract all unique words from questions and answers in all tables."""
        cursor = conn.cursor()
        tables = ["general_questions", "group_questions", "series_questions"]
        words=set()
        for table in tables:
            cursor.execute(f"SELECT intrebare, raspuns FROM {table}")
            for q, a in cursor.fetchall():
                if q:
                    for word in q.lower().split(" "):
                        words.add(word)
                if a:
                    for word in a.lower().split(" "):
                        words.add(word)        
        return words
        

#verify if the words are in the "dic"
    def verifyDic(self,word):
        x=[]
        f=open("ro.dic")
        line=f.readline()
        while line:
            word1=line.split(" ")
            #print(word1[0])
            line=f.readline()
            x.append(word1[0])
        #print(x)
        if word not in x:
            return False
        else:
            return True


    def addPhoneticTranslation(self,conn):
        print("Words that are not in the dic: ")
        phoneticTrans=PhoneticTranslation()
        for word in self.get_words_from_db(conn):
            cleaned=self.clean_word(word)
            if not cleaned:
                continue
            if not self.verifyDic(cleaned):
                w1=phoneticTrans.romanian_to_cmu(cleaned)
                with open(self.dic_file,"a") as f:
                    f.write(cleaned + " " + w1 + "\n")
                print(f"Values:{w1} for {cleaned}")
                self.existing_words.add(cleaned)
    
    def showExistingWords(self):
        return self.existing_words
                


updateDic=UpdateDic("ro.dic","students_db.db")
conn=sqlite3.connect("students_db.db")    
# #print(f"Is the word in the dic: {updateDic.verifyDic("baritiu")}")

# #print(f"The set of question response is: {updateDic.get_words_from_db(conn)} ")
updateDic.addPhoneticTranslation(conn)
print(f"Existing words: {updateDic.existing_words}")

   