import glob
import time
import tkinter.filedialog
import string
import os
from tkinter import *
import sys
import csv
# from scipy import *
from numpy import *
from read_wtf import *

update_step = 300 ## numnber of seconds between frames
hold_ms = 10    ## number of ms to hold each frame

exit = False
file_opt = {'defaultextension':'.wtf'}

class Spreadsheet:
    def __init__(self, lines):
        self.lines = lines

    def _getCell(self, row, col):
        out = None
        if row < len(self.lines):
            l = self.lines[row]
            if col < len(l):
                out = l[col]
        return out
    def getCell(self, cellstr):
        j, i = self.parsecell(cellstr)
        return self._getCell(j, i)
    def _getRegion(self, row, col, length, width):
        out = []
        i = row - 1
        while i < row + length:
            i += 1
            out.append([])
            j = col - 1
            while j < col + width:
                j += 1
                next = self._getCell(i, j)
                if next is not None:
                    out[-1].append(next)
                else:
                    break
            if length == inf and len(out[-1]) == 0:
                break
        return out
    @staticmethod
    def parsecell(cell_str):
        cell_str = cell_str.upper()
        i = 0
        if cell_str[0] == '*':
            col = inf
        else:
            col = ord(cell_str[0]) - ord('A') + 1
            for i, l in enumerate(cell_str[1:]):
                if l in string.ascii_uppercase:
                    col = col * 26  + ord(l) - ord('A') + 1
                else:
                    break
        if cell_str[i + 1:] == '*':
            row = inf
        else:
            row = int(cell_str[i + 1:])
        return row - 1, col - 1
        
    def getRegion(self, reg_str):
        'examples  r11, R11:Q23 R11:R23 R11:R*  R11:*11 R11:**'
        reg_str = reg_str.upper()
        if ':' in reg_str:
            start, stop = [self.parsecell(x) for x in reg_str.split(':')]
            out = self._getRegion(start[0], start[1], 
                                  stop[0] - start[0], stop[1] - start[1])
        else:
            out = [[self._getCell(*self.parsecell(reg_str))]]
        return out
        
    
def bitmap(csvfile):
    data = readwtf(csvfile)
    import pylab as pl
    pl.pcolormesh(data['data'][::-1], cmap='binary_r')
    words = data['words']
    words = [str(w, 'utf-8') for w in words]
    locs, tics = pl.xticks(arange(len(words)) + .5, words, rotation=90)
    pl.yticks(arange(0, 288, 12), ['%d:00' % i for i in range(24)[::-1]])
    pl.ylim(0, 288)
    pl.xlim(0, len(words))
    pl.show()

class ScreenJr:
    def __init__(self, n_col=16):
        self.buffer = zeros(n_col, 'uint8')

    def __getitem__(self, idx):
        return self.buffer[idx]

    def setPixel(self, row, col, val):
        if color:
            self.buffer[col] &= (1 << row)
        else:
            self.buffer[col] |= (1 << row)

    def getPixel(self, row, col):
        out = self.buffer[col] & (1 << col)
        return out
    def clearall(self):
        self.buffer *= 0


OFF = '#202020'
ON = '#FFFFFF'
my_inch = 40.
XOFF = 1.5 * my_inch
YOFF = 1.85 * my_inch
dx = .4 * my_inch
dy = .7 * my_inch


def nop():
    pass

def tohtml(rgb):
    r, g, b = rgb
    return '#%02x%02x%02x' % (r, g, b)

class ClockIOT:
    def __init__(self, wtf, font=('Orbitron', 20), save_images=False, dt=300, bg_color="#000000"):
        # def simulate(csvfile, font=('Kranky', 20)):
        self.display_second = 86400 #  - 300 * 4
        self.update_step = dt

        self.wtf = wtf
        self.readwtf(wtf)
        self.last_update = 0
        self.minimum_update_time = .25
        self.save_images = save_images
        self.img_num = 0
        self.bg_color = bg_color
        self.on = (int(ON[1:3], 16), 
                   int(ON[3:5], 16), 
                   int(ON[5:7], 16))
        keep = .8
        self.off = (int(int(self.bg_color[1:3], 16) * keep + self.on[0] * (1 - keep)),
                    int(int(self.bg_color[3:5], 16) * keep + self.on[1] * (1 - keep)),
                    int(int(self.bg_color[5:7], 16) * keep + self.on[2] * (1 - keep)))

        if self.save_images:
            self.movie_dir = wtf[:-4]
            print('saving images in', self.movie_dir)
            if not os.path.exists(self.movie_dir):
                os.mkdir(self.movie_dir)
            else:
                for png in glob.glob("%s/*.png" % self.movie_dir):
                    os.unlink(png)
        labels = []

        tk = Tk()
        tk.tk_setPalette(self.bg_color)
        tk.title('ClockIOT')
        self.r = Frame(tk, background=self.bg_color)
        self.can = Canvas(self.r, width=9*my_inch, height=9*my_inch)
        self.can.bind("<Button-3>", self.time_forward)
        self.can.bind("<Button-1>", self.time_backward)
        tk.bind("<Key>", self.key_press)
        self.r.pack()
        self.tk = tk
        self.makemenu()
        size = int(my_inch / 25. * 10.)
        self.did = self.can.create_text(XOFF + 9 * dx, YOFF -1 * dy, text="00:00:00", font=('Digital-7 Mono', size), fill=tohtml(self.on))
        fontname = 'Times'
        self.can.create_text(XOFF + 7 * dx, 
                             YOFF + 9 * dy, text='%s:%s' %(self.wtf[:-4], fontname), font=(fontname, 10), fill=tohtml(self.on))
        all_labels_off = {}
        for row in range(self.N_ROW):
            for col in range(self.N_COL):
                # all_labels_off[row, col] = self.can.create_text(XOFF + dx * col, YOFF + dy * row, text=self.letters[row][col], font=font, fill=OFF)
                all_labels_off[row, col] = self.can.create_text(XOFF + dx * col, YOFF + dy * row, text=self.letters[row][col], font=font, fill=tohtml(self.off))
        all_labels_on = {}
        self.labels_on = {}

        for i in range(self.N_ROW):
            for j in range(self.N_COL):
                all_labels_on[i, j] = self.can.create_text(XOFF + dx * j, YOFF + dy * i, text=self.letters[i][j], font=font, fill=tohtml(self.on))
                self.can.itemconfigure(all_labels_on[i, j], state='hidden')
        self.can.pack()
        # display = Label(r, text='00:00', font=('DS-Digital', 20))
        self.display = Label(self.r, text='00:00:00', font=('Digital-7 Mono', 15))
        self.all_labels_on = all_labels_on
        self.all_labels_off = all_labels_off
        self.after_id = self.r.after(1, self.next_time)
        self.tk.protocol("WM_DELETE_WINDOW", self.destroy)
        # self.pause()
        self.tk.mainloop()
    
    def redraw_letters(self):
        for i in range(self.N_ROW):
            for j in range(self.N_COL):
                self.can.itemconfigure(self.all_labels_off[i, j], text=self.letters[i][j])
                self.can.itemconfigure(self.all_labels_on[i, j], state='hidden', text=self.letters[i][j])
        self.labels_on = {}
        self.next_time()
                
    def readwtf(self, wtf):
        try:
            self.data = readwtf(wtf)
            self.wtf = wtf
        except AssertionError:
            raise
            self.askopenfilename()
        self.letters = self.data['letters']
        self.n_word = len(self.data['words'])
        self.N_ROW = 8
        self.N_COL = 16
        print('author:', self.data['author'])
        print('email:', self.data['email'])
        print('licence:', self.data['licence'])
        print('description:\n', self.data['desc'])
    def askopenfilename(self, *args, **kw):   
        """Returns an opened file in read mode.
        This time the dialog just returns a filename and the file is opened by your own code.
        Credit: http://tkinter.unpythonic.net/wiki/tkFileDialog
        """

        self.tk.tk_setPalette('#888888')
        save_update_step = self.update_step
        self.update_step = 0
        filename = tkinter.filedialog.askopenfilename(parent=self.tk, **file_opt)
        if filename:
            self.readwtf(filename)
            self.wtf = filename
            self.redraw_letters()
        self.update_step = save_update_step
        self.tk.tk_setPalette('#000000')
    def cconvert(self):
        self.tk.tk_setPalette('#888888')
        save_update_step = self.update_step
        self.update_step = 0
        outfn = None
        while outfn is None:
            default_fn = self.data['filename'][:-3] + 'h'
            default_fn = os.path.split(default_fn)[1]
            outfn = tkinter.filedialog.asksaveasfilename(
                filetypes=[('h files', '.h')],
                title='Save H-file',
                initialfile=(default_fn).lower())
            
            if outfn:
                cconvert(self.wtf, 8, outfn)
        self.update_step = save_update_step
        self.tk.tk_setPalette('#000000')

    def makemenu(self):
    ##  Make menu
        # create a toplevel menu
        menubar = Menu(self.tk)
        # menubar.add_command(label="Hello!", command=none)
        # menubar.add_command(label="Quit!", command=tk.quit)

        filemenu = Menu(menubar, tearoff=0)
        filemenu.add_command(label="Open", command=self.askopenfilename)
        filemenu.add_command(label="C-Convert", command=self.cconvert)
        menubar.add_cascade(label="File", menu=filemenu)

        fontmenu = Menu(menubar, tearoff=0)
        menubar.add_cascade(label="TBD", menu=fontmenu)

        langmenu = Menu(menubar, tearoff=0)
        langmenu.add_command(label="TBD", command=nop)
        menubar.add_cascade(label="Lang", menu=langmenu)

        helpmenu = Menu(menubar, tearoff=0)
        helpmenu.add_command(label="About", command=nop)
        menubar.add_cascade(label="Help", menu=helpmenu)
        # display the menu
        self.tk.config(menu=menubar)
        self.menu=menubar

    def time_forward(self, event):
        self.pause()
        self.display_second += 300

    def time_backward(self, *args, **kw):
        self.pause()
        self.display_second -= 300

    def do_uppercase(self):
        for i, j in self.all_labels_off:
            self.can.itemconfigure(self.all_labels_on[i, j], text=self.letters[i][j].upper())        
            self.can.itemconfigure(self.all_labels_off[i, j], text=self.letters[i][j].upper())

    def do_lowercase(self):
        for i, j in self.all_labels_off:
            self.can.itemconfigure(self.all_labels_on[i, j], text=self.letters[i][j].lower())        
            self.can.itemconfigure(self.all_labels_off[i, j], text=self.letters[i][j].lower())

    def key_press(self, event):
        if event.char == ' ':
            self.toggle_pause()
        elif event.char == 'H':
            self.pause()
            self.display_second += 3600
        elif event.char == 'h':
            self.pause()
            self.display_second -= 3600
        elif event.char == 'F':
            self.pause()
            self.display_second += 60 * 5
        elif event.char == 'f':
            self.pause()
            self.display_second -= 60 * 5
        elif event.char == 'M':
            self.pause()
            self.display_second += 60
        elif event.char == 'm':
            self.pause()
            self.display_second -= 60
        elif event.char == 'S':
            self.pause()
            self.display_second += 15
        elif event.char == 's':
            self.pause()
            self.display_second -= 15
        elif event.char == '.':
            self.pause()
            self.display_second += 1
        
        elif event.char == 'U':
            self.do_uppercase()
        elif event.char == 'l':
            self.do_lowercase()

        
    def toggle_pause(self):
        if self.update_step == 0: ## unpause
            self.unpause()
        else: ## pause
            self.pause()
    def pause(self):
        self.save_update_step = self.update_step
        self.update_step = 0
        
    def unpause(self):
        self.update_step = self.save_update_step

    def next_time(self): # every minute
        now = time.time()
        if now - self.last_update > self.minimum_update_time:
            self.last_update = now
            self.display_second += self.update_step
            self.display_second %= 86400
            minute = self.display_second // 60
            time5 = minute // 5
            if self.data['n_min_state'] > 0:
                min_hack_inc = int((self.display_second % 300 ) / (300. / self.data['n_min_state']))
            else:
                min_hack_inc = 0

            tm = '%02d:%02d:%02d' % (minute / 60, minute % 60, self.display_second % 60)
            self.can.itemconfig(self.did, text=tm)
            # display.config(text=tm)
            self.sequence_leds(time5, min_hack_inc)
            if True:
                if self.save_images:
                    if self.update_step == 300:
                        self.img_num = time5
                    else:
                        self.img_num += 1

                    fn = '%s/%04d.png' % (self.movie_dir, self.img_num)
                    if not os.path.exists(fn) and sys.platform.startswith('win'): # windows
                        import ImageGrab
                        
                        geom =self.tk.geometry()
                        size, x, y = geom.split('+')
                        w, h = size.split('x')
                        ImageGrab.grab((int(x), int(y), 
                                        int(x) + int(w) + 10, int(y) + int(h) + 30)).save(fn)
                    elif not os.path.exists(fn): ## linux
                        print(fn)
                        os.system('import -window ClockIOT %s' % fn)
                    if time5 == 288:
                        self.save_images = False
                    ## use convert to create animated gifs I.E.
                    ## > convert -delay 50 Hungarian_v1/*.png Hungarian_v1.gif
                    

        if exit:
            self.tk.destroy()
        else:
            self.after_id = self.r.after(hold_ms, self.next_time)
        
    def turn_on(self, i, j):
        if (i, j) in self.labels_on:
            pass
        else:
            self.labels_on[i, j] = self.all_labels_on[i, j]
            self.can.itemconfigure(self.all_labels_on[i, j], state='normal')
    def turn_off(self, i, j):
        if (i, j) in self.labels_on:
            self.can.itemconfigure(self.all_labels_on[i, j], state='hidden')
            del self.labels_on[i, j]
    def set_pixel(self, i, j, state):
        if state:
            self.turn_on(i, j)
        else:
            self.turn_off(i, j)
        for l in self.labels_on:
            self.can.delete(l)
    def sequence_leds(self, time5, min_hack_inc):
        '''
        Display the time5th time sentence`
        time5 -- five minute time increment, 0 -- 00:00, 1 -- 00:05 and so on.
        '''
        if True:
            word_state = self.data['data'][time5][:self.n_word]
            for i in where(1 - word_state)[0]:
                bit = False
                row = self.data['rows'][i] 
                col = self.data['cols'][i]
                length = self.data['lens'][i]
                for x in range(self.data['lens'][i]):
                    self.set_pixel(row, col + x, bit)
            for i in where(word_state)[0]:
                bit = True
                row = self.data['rows'][i] 
                col = self.data['cols'][i]
                length = self.data['lens'][i]
                # print self.data['words'][i], row, col, length
                for x in range(self.data['lens'][i]):
                    self.set_pixel(row, col + x, bit)
            if self.data['n_min_state'] > 0:
                for i, bit in enumerate(self.data['min_bitmap'][min_hack_inc]):
                    self.set_pixel(self.data['min_rows'][i], self.data['min_cols'][i], bit)

    def destroy(self, *args, **kw):
        self.r.after_cancel(self.after_id)
        self.tk.destroy()

def uint8_2_bits(uint):
    s = '0b'
    for i in range(7, -1, -1):
        if uint >> i & 1:
            s += '1'
        else:
            s += '0'
    return s

def bits2int(bits):
    return int(sum([b * 2 ** i for i, b in enumerate(bits)]))

def bits2bytes(bits):
    if len(bits) % 8 == 0:
        n = len(bits) // 8
    else:
        n = len(bits) // 8 + 1
    out = []
    for i in range(n):
        out.append(bits2int(bits[i * 8:(i + 1) * 8]))
    return out

def cconvert(filename, n_row=8, outfilename=None):
    data = readwtf(filename, n_row=n_row)
    if outfilename is None:
        outfile = sys.stdout
    else:
        outfile = open(outfilename, 'w')

    name = os.path.split(filename)[1].lower()[:-4]
    
    words = data['words']
    l = len(words)
    xs = data['cols']
    ys = data['rows']
    lens = data['lens']
    if l % 8 == 0:
        n_word = l
    else:
        n_word = (l // 8 + 1) * 8
    print('''#ifndef %s_H
#define %s_H''' % (name.upper(), name.upper()), file=outfile)

    print('/*', file=outfile)
    print(' * ClockIOT faceplate file.', file=outfile)
    print(' * Autogenerated from %s' % os.path.split(filename)[1], file=outfile)
    print(' * ', file=outfile)
    print(' *      Author:', data['author'], file=outfile)
    print(' *     Licence:', data['licence'], file=outfile)
    print(' * Description:', file=outfile)
    print(' *    ' + ' *    '.join(data['desc'].splitlines()), file=outfile)
    print(' * ', file=outfile)
    print(' */', file=outfile)

    print('static uint8_t %s_words[] = {' % name, file=outfile)
    print('    %3d, // # words' % n_word, file=outfile) ## #words on faceplate
    for i in range(n_word):
        if i < l:
            x = xs[i]
            y = ys[i]
            ln = lens[i]
            print('    %3d,%3d,%3d,' % (x, y, ln), end=' ', file=outfile)
        else:
            print('      0,  0,  0,', end=' ', file=outfile)
        if i % 4 == 3:
            print("    // words", file=outfile)
    print('};', file=outfile)
    print('', file=outfile)
    print('static uint8_t %s_displays[] = {' % name, file=outfile)
    print('   %d, // number of bytes per state' % (n_word // 8), file=outfile)
    mlen = max(lens)

    if  len(words) != n_word:
        words.extend(['' for i in range(8 - len(words) % 8)])
    ## print words in columns, right most col is first word
    padded_words = [w.rjust(mlen) for w in words]
    assert len(padded_words) % 8 == 0
    ### VOODOO HERE
    tmp_pw = ['' for w in padded_words]
    for j in range(n_word):
        # padded_words.insert(k + j, list(' ' * mlen))
        byte_num = j // 8
        bit_num = j % 8
        tmp_pw[byte_num * 8 + 8 - bit_num - 1] = padded_words[j]
    padded_words = tmp_pw
    i = n_word  - n_word % 8
    while i > 0:
        padded_words.insert(i, ' ')
        padded_words.insert(i, ' ')
        padded_words.insert(i, ' ')
        padded_words.insert(i, ' ')
        i -= 8
    padded_words = [list(w.rjust(mlen)) for w in padded_words]
        
    ws = transpose(padded_words)
    word_strings = []
    for l in ws:
        word_strings.append('//    ' + ''.join(l))
    ## flip bytes then print (TODO)
    for l in word_strings:
        print(l, file=outfile)
    print('   ', end=' ', file=outfile)
    for i, bits in enumerate(data['data']):
        bytes = bits2bytes(bits)
        for val in bytes:
            print('%s,' % uint8_2_bits(val), end=' ', file=outfile)
        print("\n   ", end=' ', file=outfile)
    print('};', file=outfile)
    print('// Minutes hack constants', file=outfile)
    print('static uint32_t %s_minute_leds[] = {' % name, file=outfile)
    print('  // n_minute_state, n_minute_led,        led0, led2,           led3,           led4...', file=outfile)
    print('                 %2d,           %2d,' % (data['n_min_state'], data['n_min_led']), end=' ', file=outfile)
    for i, (row_num, col_num) in enumerate(zip(data['min_rows'], data['min_cols'])):
        if i % 8 == 0:
            print('\n   ', end=' ', file=outfile)
        led_val = (row_num << 4) | col_num
        print('0x%02x,' % led_val, end=' ', file=outfile)
    print('\n};', file=outfile)
    print('static uint32_t %s_minutes_hack[] = {' % name, file=outfile)
    if data['min_bitmap'] is None:
        pass
    else:
        for state in data['min_bitmap']:
            print('    0b' + ''.join(map(str, state[::-1])) + ',', file=outfile)
    print('};', file=outfile)
    print('static Faceplate %s(%s_words, %s_displays, %s_minute_leds, %s_minutes_hack, "%s");' %(
        name, name, name, name, name, name.title()), file=outfile)
    print('#endif',file=outfile)

def timelist(fn):
    data = readwtf(fn)
    words = data['words']
    for i, l in enumerate(data['data']):
        print('%02d:%02d' % divmod(i * 5, 60), end=' ')
        for j, bit in enumerate(l):
            if bit:
                print(words[j], end=' ')
        print()
                                                    
usage = '>Simulate.py wtf font save dt'
if __name__ == '__main__':
    print(usage)
    n_row = 8
    if len(sys.argv) > 1:
        fn = sys.argv[1]
    else:
        fn = 'English_v3.wtf'
    if len(sys.argv) > 2:
        fontname = sys.argv[2]
    else:
        fontname = 'David'
        fontname = 'Times'
    if len(sys.argv) > 3:
        save_images = sys.argv[3] == 'True'
    else:
        save_images = False
    if len(sys.argv) > 4:
        dt = float(sys.argv[4])
    else:
        dt = 300

    fontsize = int(my_inch / 25. * 10)
    ClockIOT(fn, (fontname, fontsize), save_images=save_images, dt=dt)
    # bitmap(fn)
    # cconvert(fn, (fontname, 20), 'bad.h')
    # timelist(fn)
    
