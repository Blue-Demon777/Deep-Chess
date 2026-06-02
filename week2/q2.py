import copy  # use it for deepcopy if needed
import math
import logging

logging.basicConfig(format='%(levelname)s - %(asctime)s - %(message)s', datefmt='%d-%b-%y %H:%M:%S',
                    level=logging.INFO)

value_dict={}
visited_states_count=0

class History:
    def __init__(self, num_boards=3, history=None):
        """
        # self.history : Eg: [0, 4, 2, 5]
            keeps track of sequence of actions played since the beginning of the game.
            Each action is an integer between 0-(9n-1) representing the square in which the move will be played as shown
            below (n=2 is the number of boards).

             Board 1
              ___ ___ ____
             |_0_|_1_|_2_|
             |_3_|_4_|_5_|
             |_6_|_7_|_8_|

             Board 2
              ____ ____ ____
             |_9_|_10_|_11_|
             |_12_|_13_|_14_|
             |_15_|_16_|_17_|

        # self.boards
            empty squares are represented using '0' and occupied squares are 'x'.
            Eg: [['x', '0', 'x', '0', 'x', 'x', '0', '0', '0'], ['0', 0', '0', 0', '0', 0', '0', 0', '0']]
            for two board game

            Board 1
              ___ ___ ____
             |_x_|___|_x_|
             |___|_x_|_x_|
             |___|___|___|

            Board 2
              ___ ___ ____
             |___|___|___|
             |___|___|___|
             |___|___|___|

        # self.player: 1 or 2
            Player whose turn it is at the current history/board

        :param num_boards: Number of boards in the game of Notakto.
        :param history: list keeps track of sequence of actions played since the beginning of the game.
        """
        self.num_boards = num_boards
        if history is not None:
            self.history = history
            self.boards = self.get_boards()
        else:
            self.history = []
            self.boards = []
            for i in range(self.num_boards):
                # empty boards
                self.boards.append(['0', '0', '0', '0', '0', '0', '0', '0', '0'])
        # Maintain a list to keep track of active boards
        self.active_board_stats = self.check_active_boards()
        self.current_player = self.get_current_player()

    def get_boards(self):
        """ Play out the current self.history and get the boards corresponding to the history.

        :return: list of lists
                Eg: [['x', '0', 'x', '0', 'x', 'x', '0', '0', '0'], ['0', 0', '0', 0', '0', 0', '0', 0', '0']]
                for two board game

                Board 1
                  ___ ___ ____
                 |_x_|___|_x_|
                 |___|_x_|_x_|
                 |___|___|___|

                Board 2
                  ___ ___ ____
                 |___|___|___|
                 |___|___|___|
                 |___|___|___|
        """
        boards = []
        for i in range(self.num_boards):
            boards.append(['0', '0', '0', '0', '0', '0', '0', '0', '0'])
        for i in range(len(self.history)):
            board_num = math.floor(self.history[i] / 9)
            play_position = self.history[i] % 9
            boards[board_num][play_position] = 'x'
        return boards

    def get_valid_actions(self):
        actions=[]
        order=[4,0,2,6,8,1,3,5,7]
        for b in range(self.num_boards):
            if self.active_board_stats[b]==0:
                continue
            for pos in order:
                if self.boards[b][pos]=='0':
                    actions.append(9*b+pos)
        return actions
    
    def check_active_boards(self):
        """ Return a list to keep track of active boards

        :return: list of int (zeros and ones)
                Eg: [0, 1]
                for two board game

                Board 1
                  ___ ___ ____
                 |_x_|_x_|_x_|
                 |___|_x_|_x_|
                 |___|___|___|

                Board 2
                  ___ ___ ____
                 |___|___|___|
                 |___|___|___|
                 |___|___|___|
        """
        active_board_stat = []
        for i in range(self.num_boards):
            if self.is_board_win(self.boards[i]):
                active_board_stat.append(0)
            else:
                active_board_stat.append(1)
        return active_board_stat

    @staticmethod
    def is_board_win(board):
        for i in range(3):
            if board[3 * i] == board[3 * i + 1] == board[3 * i + 2] != '0':
                return True

            if board[i] == board[i + 3] == board[i + 6] != '0':
                return True

        if board[0] == board[4] == board[8] != '0':
            return True

        if board[2] == board[4] == board[6] != '0':
            return True
        return False

    def get_current_player(self):
        total_num_moves = len(self.history)
        if total_num_moves % 2 == 0:
            return 1
        else:
            return -1

    def get_boards_str(self):
        boards_str = ""
        for i in range(self.num_boards):
            boards_str = boards_str + ''.join([str(j) for j in self.boards[i]])
        return boards_str
    
    def get_value(self):
        for x in self.active_board_stats:
            if x==1:
                return 0
        return self.get_current_player()
        
        

def alphabeta(history_obj,alpha,beta):
    global value_dict
    global visited_states_count
    visited_states_count+=1
    key=history_obj.get_boards_str()
    if key in value_dict:
        return value_dict[key]
    value=history_obj.get_value()
    if value!=0:
        value_dict[key]=value
        return value
    actions=history_obj.get_valid_actions()
    if history_obj.get_current_player()==1:
        best=-math.inf
        for a in actions:
            child=History(history_obj.num_boards,history_obj.history+[a])
            val=alphabeta(child,alpha,beta)
            best=max(best,val)
            alpha=max(alpha,best)
            if alpha>=beta:
                break
    else:
        best=math.inf
        for a in actions:
            child=History(history_obj.num_boards,history_obj.history+[a])
            val=alphabeta(child,alpha,beta)
            best=min(best,val)
            beta=min(beta,best)
            if alpha>=beta:
                break
    value_dict[key]=best
    return best

import pygame

CELL_SIZE=60
PADDING=20
BOARD_GAP=40
GRID_SIZE=3*CELL_SIZE

def draw(screen,history_obj):
    screen.fill((30,30,30))
    for b in range(history_obj.num_boards):
        bx=b*(GRID_SIZE+BOARD_GAP)+PADDING
        by=PADDING
        for i in range(9):
            x=bx+(i%3)*CELL_SIZE
            y=by+(i//3)*CELL_SIZE
            pygame.draw.rect(screen,(200,200,200),(x,y,CELL_SIZE,CELL_SIZE),2)
            if history_obj.boards[b][i]=='x':
                pygame.draw.line(screen,(255,0,0),(x+10,y+10),(x+CELL_SIZE-10,y+CELL_SIZE-10),2)
                pygame.draw.line(screen,(255,0,0),(x+CELL_SIZE-10,y+10),(x+10,y+CELL_SIZE-10),2)
    pygame.display.flip()

def get_clicked_cell(pos,num_boards):
    x,y=pos
    for b in range(num_boards):
        bx=b*(GRID_SIZE+BOARD_GAP)+PADDING
        by=PADDING
        if bx<=x<bx+GRID_SIZE and by<=y<by+GRID_SIZE:
            cx=(x-bx)//CELL_SIZE
            cy=(y-by)//CELL_SIZE
            return b*9+(cy*3+cx)
    return None

def bot_move(history_obj):
    best_val = -math.inf if history_obj.current_player == 1 else math.inf
    best_a = None
    for a in history_obj.get_valid_actions():
        child = History(history_obj.num_boards, history_obj.history + [a])
        val = alphabeta(child, -math.inf, math.inf)
        if history_obj.current_player == 1:
            if val > best_val:
                best_val = val
                best_a = a
        else:
            if val < best_val:  
                best_val = val
                best_a = a
    return best_a

def play_notakto(num_boards=2,human_player=-1):
    pygame.init()
    w=num_boards*(GRID_SIZE+BOARD_GAP)+PADDING
    h=GRID_SIZE+2*PADDING
    screen=pygame.display.set_mode((w,h))

    state=History(num_boards=num_boards)
    last_player=None

    running=True
    while running:
        draw(screen,state)

        if len(state.get_valid_actions())==0:
            print("GAME OVER")
            break

        last_player=state.get_current_player()

        if state.get_current_player()!=human_player:
            a=bot_move(state)
            if a is None:
                break
            state=History(num_boards,state.history+[a])
            continue

        for event in pygame.event.get():
            if event.type==pygame.QUIT:
                running=False
            if event.type==pygame.MOUSEBUTTONDOWN:
                a=get_clicked_cell(event.pos,num_boards)
                if a in state.get_valid_actions():
                    state=History(num_boards,state.history+[a])
    pygame.quit()

    # After the game loop ends cleanly (no quit):
    if len(state.get_valid_actions()) == 0:
        # whoever made the last move loses
        # odd number of total moves → player 1 made the last move
        # even number → player 2 made the last move
        last_mover = 1 if len(state.history) % 2 == 1 else -1
        if last_mover == human_player:
            print("HUMAN LOSES")
        else:
            print("BOT LOSES, HUMAN WINS")
    
if __name__=="__main__":
    root=History(num_boards=2,history=[])
    value=alphabeta(root,-math.inf,math.inf)
    print("FINAL VALUE:",value)
    print("STATES VISITED:",visited_states_count)
    print("UNIQUE STATES:",len(value_dict))

    print("Choose side:")
    print("1 -> Player 1")
    print("2 -> Player 2")
    x=int(input().strip())
    human_player=1 if x==1 else -1

    play_notakto(2,human_player)
