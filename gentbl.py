


import numpy
import os

# create "Tables" folder
root_folder = "Tables"
os.system('rm -rf %s' % root_folder)
os.mkdir(root_folder)

# tables to generate
demo_table = 
{
    'name': 'demo', 
    'columns': 
    {
        'x': {'type': 'dbl', 'count': 20}, 
        'y': {'type': 'dbl', 'count': 20}, 
        'z': {'type': 'dbl', 'count': 20}
    }
}


tables = [demo_table]
# fixed seed
numpy.random.seed(37)

# generate table data
for table in tables:
    # create metadata file (.tbl)
    name = table['name']
    ddl_file = open(os.path.join(root_folder, name + '.tbl'), 'w+')
    column_data = table['columns']
    for colname,coldata in column_data.iteritems():
        # metadata is structured as 'name type count'
        ddl_file.write('%s %s %d\n' % (colname, coldata['type'], coldata['count']))
    ddl_file.close()
    # create data files for each column
    os.mkdir(os.path.join('Tables', name))
    for colname,coldata in column_data.iteritems():
        col_file = open(os.path.join('Tables', name, colname + '.col'), 'wb+')
        randdata = numpy.random.randint(0, 2^30, size=(coldata['count'],))
        if coldata['type'] == 'int':
            randdata.astype('int32').tofile(col_file)
        elif coldata['type'] == 'lng':
            randdata.astype('int64').tofile(col_file)
        elif coldata['type'] == 'flt':
            randdata.astype('float32').tofile(col_file)
        elif coldata['type'] == 'dbl':
            randdata.astype('float64').tofile(col_file)
        print(name, colname, randdata[:10])
        col_file.close()
