#!/usr/bin/env python
import os
import re
import argparse
import subprocess

import csv
import matplotlib.pyplot as plt

import traceback
pause = lambda s='*': raw_input('\n[%s] Press any key to continue (%s)' % (s, traceback.extract_stack(None, 2)[0][2]))

DEBUG = False


def compare(original, compare_object, height, width, frames, output_name):
    cmdline = str('%s %s %s %d %d %d 1 %s PSNR SSIM'
                % ('./vqmt', original, compare_object, height, width, frames,
                   output_name))
    p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)
    print(p.communicate()[1])


def read_results_from_csv(output_name, type, color):
    csv_name = output_name+'_' + type + '.csv'
    csv_file = open(csv_name, 'rb')
    reader = csv.reader(csv_file, dialect='excel')
    idx = []
    result = []
    for row in reader:
        if row[1] == 'inf':
            row[1] = 0
        if row[0] != 'frame' and row[0] != 'average':
            idx.append(int(row[0]))
            result.append(float(row[1]))
    #    print row
    #_, content = get_result_from_csv(csv_name)
    #result = [[items[0], items[1]] for items in reader]
    return [output_name, color, idx, result]


def plot_one_results(results,filename):
    fig, axes = plt.subplots(1, 1, sharex=True)
    for result in results:
        axes.plot(result[2], result[3], marker='.', label=result[0], color=result[1])
    plt.savefig(str(filename)+'_Result.png')
    plt.show()


class cOneEcTest(object):
    def __init__(self, file264, width, height, keyframe, original):
        self.file264 = file264
        self.width = int(width)
        self.height = int(height)
        self.keyframe = int(keyframe)
        self.original = original
        self.err_pos = self.keyframe/2
        self.err_length = 1

        self.origin_rec_yuv = file264+'.yuv'
        self.origin_rec_log = file264+'_dec.txt'
        cmdline = str('%s %s %s 2> %s'
                    % ('./h264dec', self.file264, self.origin_rec_yuv, self.origin_rec_log))
        p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)
        print(p.communicate()[1])

        # the number of frames
        match_re = re.compile(r'Frames:\s+?(\d+)')
        lines = open(self.origin_rec_log).readlines()
        for line in lines:
            r = match_re.search(line)
            if r is not None:
                self.frames = int(r.groups()[0])

        self.impaired_264 = file264+'_impaired.264'
        self.impaired_log = file264+'_impaired_log.txt'
        self.openh264_ec_yuv = file264+'_impaired.yuv'
        self.openh264_ec_tmp_yuv = file264+'_impaired_tmp.yuv'
        self.freeze_result_yuv = file264+'_freeze.yuv'
        self.drop_idx_list = []

        #
        self.results = {'Origin_Rec':[], 'Origin_Ec':[], 'Origin_Freeze':[],
                        'Rec_Ec':[], 'Rec_Freeze':[]}

    def define_error_pos(self, pos):
        self.err_pos = pos

    def define_error_length(self, length):
        self.err_length = length

    def produce_the_files(self):
        # the files
        cmdline = str('%s 1 %s %d %d %d >%s'
                    % ('./SimulateNalDrop', self.file264, self.keyframe, self.err_pos, self.err_length, self.impaired_log))
        p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)

        # the freeze results
        cmdline = str('%s 0  %d %d %s %s %d %d'
                    % ('./CopyFromFrame', self.width, self.height, self.origin_rec_yuv, self.freeze_result_yuv,
                       self.keyframe, self.err_pos))
        p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)
        print(p.communicate()[1])

        # the ec results
        cmdline = str('%s %s %s'
                    % ('./h264dec', self.impaired_264, self.openh264_ec_yuv))
        p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)
        print(p.communicate()[1])

        # deal with the dropping
        """
        match_re = re.compile(r'whole frame dropped: (\d+)')
        f = open(self.impaired_log)
        lines = f.readlines()
        for line in lines:
            r = match_re.search(line)
            if r is not None:
                self.drop_idx_list.append(int(r.groups()[0]))
        if len(self.drop_idx_list) > 0:
            for idx in self.drop_idx_list:
                cmdline = str('%s 1 %d %d %s %s %d %d'
                        % ('./CopyFromFrame', self.width, self.height, self.openh264_ec_yuv, self.openh264_ec_tmp_yuv,
                           idx, 1))
                p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)
                print(p.communicate()[1])
                os.rename(self.openh264_ec_tmp_yuv, self.openh264_ec_yuv)
        """

    def all_compare(self):
        all_results = []
        if self.original is not None:
            compare(self.original, self.origin_rec_yuv, self.height, self.width, self.frames, 'Origin_Rec')
            self.results['Origin_Rec'] = read_results_from_csv('Origin_Rec','psnr','g')

            compare(self.original, self.openh264_ec_yuv, self.height, self.width, self.frames, 'Origin_Ec')
            self.results['Origin_Ec'] = read_results_from_csv('Origin_Ec','psnr','b')

            compare(self.original, self.freeze_result_yuv, self.height, self.width, self.frames, 'Origin_Freeze')
            self.results['Origin_Freeze'] = read_results_from_csv('Origin_Freeze','psnr','r')

            all_results.append(self.results['Origin_Rec'])
            all_results.append(self.results['Origin_Ec'])
            all_results.append(self.results['Origin_Freeze'])
        else:
            compare(self.origin_rec_yuv, self.openh264_ec_yuv, self.height, self.width, self.frames, 'Rec_Ec')
            self.results['Rec_Ec'] = read_results_from_csv('Rec_Ec','psnr','b')

            compare(self.origin_rec_yuv, self.freeze_result_yuv, self.height, self.width, self.frames, 'Rec_Freeze')
            self.results['Rec_Freeze'] = read_results_from_csv('Rec_Freeze','psnr','r')

            all_results.append(self.results['Rec_Ec'])
            all_results.append(self.results['Rec_Freeze'])

        plot_one_results(all_results, self.file264)


if __name__ == '__main__':
    argParser = argparse.ArgumentParser()
    argParser.add_argument("file264", nargs='?', default=None, help="the 264 files to be processed")
    argParser.add_argument("width", nargs='?', default=None, help="width")
    argParser.add_argument("height", nargs='?', default=None, help="height")
    argParser.add_argument("--keyframe", nargs='?', default=None, help="keyframe")
    argParser.add_argument("--error_position", nargs='?', default=None, help="error_position")
    argParser.add_argument("--compare_with_original", nargs='?', default=None, help="compare with original, default None")
    args = argParser.parse_args()

    currentTest = cOneEcTest(args.file264, args.width, args.height, args.keyframe, args.compare_with_original)
    if args.error_position is not None:
        currentTest.define_error_pos(int(args.error_position))

    currentTest.produce_the_files()
    currentTest.all_compare()









