import numpy as np
import pandas as pd
from glob2 import glob
from sqlalchemy import create_engine



csvin = {}

for csvname in sorted(glob("*.txt2.csv")):
    with open(csvname,"r") as f:
        csvin[csvname] = pd.read_csv(f)

allcsv = pd.concat(csvin)

import mysql.connector

# Credentials to database connection
hostname="waterdata.glwi.uwm.edu"
dbname="metasys"
uname="tomh"
pwd="wd34faer"

# Create SQLAlchemy engine to connect to MySQL Database

engine = create_engine("mysql://tomh:wd34faer@waterdata.glwi.uwm.edu/metasys")

#con = mysql.connector.connect(host=hostname,user=uname,password=pwd,database=dbname)
con = engine.connect()


#con.database.set("metasys")

# Convert dataframe to sql table                                   
allcsv.to_sql('metasys.allpoints_postchange_2022', con, index=False)

