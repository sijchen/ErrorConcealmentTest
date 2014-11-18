#!/usr/bin/env python
import os, shutil, sys
import re
import argparse
import subprocess
import glob

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
    def __init__(self, file264, decoder_list):
        self.file264 = file264
        self.decoder_list = decoder_list

        # process the no loss bs first
        self.origin_rec_yuv = file264+'.yuv'
        self.origin_rec_log = file264+'_dec.txt'
        cmdline = str('%s %s %s 2> %s'
                    % ('./h264dec', self.file264, self.origin_rec_yuv, self.origin_rec_log))
        p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)
        print(p.communicate()[1])
        os.remove(self.origin_rec_yuv)

        # get the number of frames
        frame_match_re = re.compile(r'Frames:\s+?(\d+)')
        width_match_re = re.compile(r'iWidth:\s+?(\d+)')
        height_match_re = re.compile(r'height:\s+?(\d+)')

        file = open(self.origin_rec_log)
        lines = file.readlines()
        for line in lines:
            r = frame_match_re.search(line)
            if r is not None:
                self.frames = int(r.groups()[0])

            r = width_match_re.search(line)
            if r is not None:
                self.width = int(r.groups()[0])

            r = height_match_re.search(line)
            if r is not None:
                self.height = int(r.groups()[0])
        file.close()

        self.impaired_264 = file264+'_impaired.264'
        self.impaired_log = file264+'_impaired_log.txt'
        self.openh264_ec_yuv = file264+'_impaired.yuv'
        self.openh264_ec_tmp_yuv = file264+'_impaired_tmp.yuv'
        self.freeze_result_yuv = file264+'_freeze.yuv'
        self.drop_idx_list = []

        #
        self.results = {'Origin_Rec':[], 'Origin_Ec':[], 'Origin_Freeze':[],
                        'Rec_Ec':[], 'Rec_Freeze':[]}

    def produce_the_files(self, target):
        sys.stdout.write('produce loss rate=%d:\n' %target)

        # the files
        testloss_cfg = "." + os.sep + "cfg" + os.sep + "%d.dat" %target
        impaired_264 = self.file264 + "_%d.264" %target
        cmdline = str('%s -ib %s -ob %s -ilosscfg %s -ol %s'
                    % ('./PacketLossTool', self.file264, impaired_264, testloss_cfg, self.impaired_log))
        sys.stdout.write('cmdline: %s\n' %cmdline)
        p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)
        print(p.communicate()[1])
        #PacketLossTool.exe -ib CVPCMNL1_SVA_C.264 -ob out.264 -iilosscfg testloss.cfg -ol outloss.cfg

        """
        # the freeze results
        cmdline = str('%s 0  %d %d %s %s %d %d'
                    % ('./CopyFromLastFrame', self.width, self.height, self.origin_rec_yuv, self.freeze_result_yuv,
                       self.keyframe, self.err_pos))
        p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=True)
        print(p.communicate()[1])
        """

        for decoder_item in self.decoder_list:
            openh264_ec_yuv = impaired_264 + "_dec_%s" %(decoder_item) + ".yuv"
            sys.stdout.write('decoding using %s: input %s output %s\n' %(decoder_item, impaired_264, openh264_ec_yuv) )
            # the ec results
            #cmdline = str('%s %s %s'
            #            % ('./%s' , self.impaired_264, ))

            cmdline = ['./%s' %decoder_item, '%s' %impaired_264,'%s' %openh264_ec_yuv]
            sys.stdout.write('cmdline: %s\n' %cmdline)
            try:
                p = subprocess.Popen(cmdline, stderr=subprocess.PIPE, shell=False)
                print(p.communicate()[1])
                os.remove(openh264_ec_yuv)
            except:
                sys.stdout.write('Error at: %s\n' %impaired_264)
                os.rename(impaired_264+"problem.264", impaired_264)



    def all_compare(self):
        all_results = []
        compare(self.origin_rec_yuv, self.openh264_ec_yuv, self.height, self.width, self.frames, 'Rec_Ec')
        self.results['Rec_Ec'] = read_results_from_csv('Rec_Ec','psnr','b')

        compare(self.origin_rec_yuv, self.freeze_result_yuv, self.height, self.width, self.frames, 'Rec_Freeze')
        self.results['Rec_Freeze'] = read_results_from_csv('Rec_Freeze','psnr','r')

        all_results.append(self.results['Rec_Ec'])
        all_results.append(self.results['Rec_Freeze'])

        plot_one_results(all_results, self.file264)


if __name__ == '__main__':
    sys.stdout.write('Example usage: -folder {folder} -decoders {list of decoders}\n')
    argParser = argparse.ArgumentParser()
    argParser.add_argument("-file", nargs='?', default=None, help="the 264 files to be processed")
    argParser.add_argument("-folder", nargs='?', default=None, help="the 264 folder to be processed")
    argParser.add_argument("-decoders", nargs='+', default=None, help="decode list")
    argParser.add_argument("--keep_after_decode", nargs='?', default=False, help="compare with original, default None")
    args = argParser.parse_args()

    if args.file is not None:
        currentTest = cOneEcTest(args.file264, args.decoders)

        for loss_rate in [3,5,10,20]:
            currentTest.produce_the_files(loss_rate)


    if args.folder is not None:
        logfiles = []
        for f in glob.glob(args.folder + os.sep + '*.264'):
            logfiles.append(f)
        # process each file
        for file in logfiles:
            currentTest = cOneEcTest(file, args.decoders)

            for loss_rate in [3,5,10,20]:
                currentTest.produce_the_files(loss_rate)






