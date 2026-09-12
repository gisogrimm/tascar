function plot_sourcedir_spec
  addpath('../scripts');
  vT = 0:0.02:1;
  vD = (vT*2-1)*180;
  mIR1 = [];
  csSrcType = {'head','cardioidmod','generic1storder','omni'};
  fh = figure('PaperUnits','centimeters','PaperPosition',[0,0,10,6*numel(csSrcType)]*1.5);
  for k=1:numel(csSrcType)
      subplot(numel(csSrcType),1,k);
      sSrcType = csSrcType{k}
      for kt=1:numel(vT)
          [mIR(:,kt),fs] = render_ir( 'sourcedir_head.tsc', vT(kt), sSrcType );
      end
      mH = 20*log10(abs(realfft(mIR)));
      vF = ([1:size(mH,1)]-1)*10;
      h = pcolor(vF,vD,mH');
      set(h,'EdgeColor','none');
      xtick = round(1000*2.^[-4:1:2]);
      set(gca,'CLim',[-40,5],'XLim',[100,8000],'XScale','log',...
	      'XTick',xtick,'XTickLabel',num2str(xtick'),...
         'YLim',[min(vD),max(vD)]);
      colorbar();
      xlabel('frequency / Hz');
      %ylabel('obstacle center / m');
      title(sSrcType);
  end
  drawnow();
  saveas(fh,'sourcedir.png','png');
  %system('epstopdf sourcedir.eps');
end

function [ir,fs] = render_ir( fname, t, sSrcType )
  sLib = 'LD_LIBRARY_PATH=../libtascar/build/:../plugins/build/';
  sApp = '../apps/build/tascar_renderir';
  sCmd = sprintf('%s SRCTYPE=%s %s -t %g -f 16000 -l 1600 -o temp.wav %s',...
		 sLib, sSrcType, sApp,t,fname);
  system(sCmd);
  [ir,fs] = audioread('temp.wav');
  system('rm -f temp.wav');
end
