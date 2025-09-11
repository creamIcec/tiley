import math

# max cells in a side of grid
row_max = col_max = 4

# list of windows
windows = []

# current max row and column

next_row = 0
next_column = 0

def increase_row_and_column(row: int, column: int):

    if row >= row_max:
        print("Maximum count of windows reached.")

    if column >= col_max - 1:
        column = 0
        row += 1
    else:
        column += 1

    return row, column

def insert_new(count: int):
    global next_row, next_column
    count += 1
    if len(windows) == 0:
        windows.append({"col": 0, "row": 0, "w": 1, "h": 1})
    else:
        windows.append({"col": next_column, "row": next_row, "w": 1, "h": 1})
    
    next_row, next_column = increase_row_and_column(next_row, next_column)
    
    return count

def reflow(count: int):
    for window in windows:
        flag_row_fill = False
        flag_column_fill = False
        if math.floor((window["row"] * col_max + window["col"]) / col_max) == math.floor(count / col_max):
            window["h"] = row_max - window["row"]
            flag_column_fill = True
        if (math.floor((window["row"] * col_max + window["col"]) / col_max) == math.floor(count / col_max) 
            and (window["row"] * col_max + window["col"] + 1 == count)):
            window["w"] = col_max - window["col"]
            flag_row_fill = True
        
        if not flag_column_fill:
            window["h"] = 1
        if not flag_row_fill:
            window["w"] = 1

if __name__ == "__main__":

    # total window count
    count = 0

    print("****Grid layout testing program****")
    while(True):
        key = input("Press any key to simulate inserting a new window...(press q to quit)\n")
        if(key != 'q'):
            count = insert_new(count)
            reflow(count)
            print("****Windows status:****")
            for window in windows:
                print(f"col:{window['col']}, row:{window['row']}, width:{window['w']}, height:{window['h']}")
            print("****Windows status end****")
        else:
            print("exiting...")
            break