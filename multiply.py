from jsonschema import validate
from jsonschema import exceptions
import json
import os

def load_schema(sch_path):
    with open(sch_path, 'r') as schemaString:
        return json.load(schemaString)

def validate_one_(path, schema):
    with open(path, 'r') as jsonString:
        try:
            validate(instance=json.load(jsonString), schema=schema)
            return ''
        except exceptions.ValidationError as e:
            return f'{e.message} at {e.absolute_schema_path}'
            #print(e.schema)
            #print(e.instance)

def validate_one(path, sch_path):
    return validate_one_(path, load_schema(sch_path))

def validate_all(path, sch_path):
    schema = load_schema(sch_path)

    ans = ''
    for file in os.listdir(path):
        filename = os.fsdecode(file)
        if filename.endswith('.json') and not filename.endswith('schema.json'): 
            ans_ = validate_one_(os.path.join(path, filename), schema)
            if len(ans_) > 0:
                ans += f'{filename}: {ans_}\n'
            continue
        else:
            continue
    return ans

#print(validate_one('json/HomieSpells_Homing.json', 'json/schema.json'))
#print(validate_all('json', 'json/schema.json'))
