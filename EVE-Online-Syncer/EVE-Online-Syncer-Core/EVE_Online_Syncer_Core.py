import types
import blue
import traceback
import time
import carbonui.const as carbonui_const
from carbonui import fontconst as carbonui_fontconst
import probescanning.formations
from eve.client.script.ui.control import eveLabel
from eve.client.script.ui.login.charSelection.characterSelection import CharacterSelection
from eve.client.script.ui.hacking.hackingWindow import HackingWindow
from eve.client.script.ui.shared.mapView.solarSystemViewPanel import SolarSystemViewPanel


global Original_HackingSvc_OnTileClicked
Original_HackingSvc_OnTileClicked = None

global Original_ScanSvc_SetSelectedSites
Original_ScanSvc_SetSelectedSites = None

class syncer_core:
    @staticmethod
    def Patch_CharacterSelection_CanLogin(self):
        return True
    
    #-------------------------------------------------------------------------------------
    #挖坟修改

    is_Hacking = False
    current_HackingWindow = None
    @staticmethod
    def Patch_HackingSvc_OnTileClicked(self, tileCoord):
        syncer_core.Recover_HackingSvc_OnTileClicked()
        if not syncer_core.is_Hacking:
            try:
                with open(r'D:\Games\EVE\Reverse Engineering\EVE-Online-Syncer\EVE-Online-Syncer\x64\Debug\log.txt', 'a+') as logfile:
                    with open(r'D:\Games\EVE\Reverse Engineering\EVE-Online-Syncer\EVE-Online-Syncer\x64\Debug\tileData_' + str(int(time.time())) + '.txt', 'a+') as tileDataFile:
                        tileDataFile.write(str(sm.GetService('hackingUI').tileDataByCoord.values()))
                    #获取破解窗口
                    syncer_core.current_HackingWindow = HackingWindow.GetIfOpen()
                    #没找到破解窗口
                    if not syncer_core.current_HackingWindow:
                        logfile.write("Can't find hacking window!\n\n")
                        logfile.close()
                        sm.GetService('hackingUI').OnTileClicked(tileCoord)
                        syncer_core.DeRecover_HackingSvc_OnTileClicked()
                        return
                    #添加节点坐标标签
                    tileKeys = syncer_core.current_HackingWindow.tilesByTileCoord.keys()
                    for key in tileKeys:
                        tile = syncer_core.current_HackingWindow.tilesByTileCoord[key]
                        x, y = tile.tileData.coord
                        coordText = '(%s, %s)' % (x, y)
                        eveLabel.Label(name='coordLabel', parent=tile, align=carbonui_const.CENTERTOP, fontsize=carbonui_fontconst.EVE_SMALL_FONTSIZE, text=coordText, idx=0)
                    #Hook破解窗口关闭函数
                    syncer_core.current_HackingWindow.Original_HackingWindow_EndGame = syncer_core.current_HackingWindow.EndGame
                    syncer_core.current_HackingWindow.EndGame = types.MethodType(syncer_core.Patch_HackingWindow_EndGame, syncer_core.current_HackingWindow)
                    logfile.write('Patch hacking window complete!\n\n')
            except Exception as ex:
                with open(r'D:\Games\EVE\Reverse Engineering\EVE-Online-Syncer\EVE-Online-Syncer\x64\Debug\log.txt', 'a+') as logfile:
                    logfile.write('------------------Error----------------------\n')
                    logfile.write(traceback.format_exc())
                    logfile.write('\n')
                    logfile.write('------------------Error----------------------\n')

            syncer_core.is_Hacking = True
        sm.GetService('hackingUI').OnTileClicked(tileCoord)
        syncer_core.DeRecover_HackingSvc_OnTileClicked()
        return
    
    @staticmethod
    def Recover_HackingSvc_OnTileClicked():
        sm.GetService('hackingUI').OnTileClicked = Original_HackingSvc_OnTileClicked
        return

    @staticmethod
    def DeRecover_HackingSvc_OnTileClicked():
        sm.GetService('hackingUI').OnTileClicked = types.MethodType(syncer_core.Patch_HackingSvc_OnTileClicked, sm.GetService('hackingUI'))
        return
    
    @staticmethod
    def Patch_HackingWindow_EndGame(self, won):
        if not syncer_core.current_HackingWindow:
            return
        syncer_core.Recover_HackingWindow_EndGame()
        syncer_core.current_HackingWindow.Original_HackingWindow_EndGame = None
        syncer_core.current_HackingWindow.EndGame(won)
        with open(r'D:\Games\EVE\Reverse Engineering\EVE-Online-Syncer\EVE-Online-Syncer\x64\Debug\log.txt', 'a+') as logfile:
            logfile.write('Hacking window close!\n\n')
        syncer_core.current_HackingWindow = None
        syncer_core.is_Hacking = False
        return
    
    @staticmethod
    def Recover_HackingWindow_EndGame():
        if not syncer_core.current_HackingWindow:
            return
        syncer_core.current_HackingWindow.EndGame = syncer_core.current_HackingWindow.Original_HackingWindow_EndGame
        return
    
    @staticmethod
    def DeRecover_HackingWindow_EndGame():
        if not syncer_core.current_HackingWindow:
            return
        syncer_core.current_HackingWindow.EndGame = types.MethodType(syncer_core.Patch_HackingWindow_EndGame, syncer_core.current_HackingWindow)
        return
        
    #-------------------------------------------------------------------------------------
    #扫描修改
    
    selected_Sites_Id = None
    @staticmethod
    def Patch_ScanSvc_SetSelectedSites(self, siteIDs):
        syncer_core.Recover_ScanSvc_SetSelectedSites()
        try:
            #获取扫描服务
            scanSvc = sm.GetService('scanSvc')
            #执行原函数
            scanSvc.SetSelectedSites(siteIDs)
            #过滤掉选择信号重复的情况
            if len(siteIDs) <= 0:
                syncer_core.selected_Sites_Id = None
                syncer_core.DeRecover_ScanSvc_SetSelectedSites()
                return
            result = syncer_core.selected_Sites_Id == siteIDs
            if (result is bool and result):
                syncer_core.DeRecover_ScanSvc_SetSelectedSites()
                return
            elif (hasattr(result, 'all') and result.all()):
                syncer_core.DeRecover_ScanSvc_SetSelectedSites()
                return
            #存储选择的信号
            syncer_core.selected_Sites_Id = siteIDs
            #获取恒星系图窗口
            current_SolarSystemViewPanel = SolarSystemViewPanel.GetIfOpen()
            if current_SolarSystemViewPanel:
                #获取探针扫描结果
                scanResult = scanSvc.GetScanResults()
                #扫描结果不为空时，将镜头和探针移到第一个选择的信号上
                if len(scanResult) > 0:
                    pos_SelectedSite = [x['pos'] for x in scanResult if x['id'] == syncer_core.selected_Sites_Id[0]]
                    if len(pos_SelectedSite) > 0:
                        current_SolarSystemViewPanel.mapView.SetCameraPointOfInterestSolarSystemPosition(session.solarsystemid2, pos_SelectedSite[0])
                        if not scanSvc.IsScanning():
                            scanSvc.MoveProbesToFormation(probescanning.formations.PINPOINT_FORMATION, initialPosition = pos_SelectedSite[0])
        except:
            with open(r'D:\Games\EVE\Reverse Engineering\EVE-Online-Syncer\EVE-Online-Syncer\x64\Debug\log.txt', 'a+') as logfile:
                logfile.write('------------------Error----------------------\n')
                logfile.write(traceback.format_exc())
                logfile.write('\n')
                logfile.write('------------------Error----------------------\n')
        syncer_core.DeRecover_ScanSvc_SetSelectedSites()
        return
    
    @staticmethod
    def Recover_ScanSvc_SetSelectedSites():
        sm.GetService('scanSvc').SetSelectedSites = Original_ScanSvc_SetSelectedSites
        return

    @staticmethod
    def DeRecover_ScanSvc_SetSelectedSites():
        sm.GetService('scanSvc').SetSelectedSites = types.MethodType(syncer_core.Patch_ScanSvc_SetSelectedSites, sm.GetService('scanSvc'))
        return

#关闭崩溃报告
blue.EnableCrashReporting(False)
#解除多开限制
CharacterSelection._CanLogin = types.MethodType(syncer_core.Patch_CharacterSelection_CanLogin, CharacterSelection)
#挖坟修改
Original_HackingSvc_OnTileClicked = sm.GetService('hackingUI').OnTileClicked
sm.GetService('hackingUI').OnTileClicked = types.MethodType(syncer_core.Patch_HackingSvc_OnTileClicked, sm.GetService('hackingUI'))
#扫描修改
Original_ScanSvc_SetSelectedSites = sm.GetService('scanSvc').SetSelectedSites
sm.GetService('scanSvc').SetSelectedSites = types.MethodType(syncer_core.Patch_ScanSvc_SetSelectedSites, sm.GetService('scanSvc'))