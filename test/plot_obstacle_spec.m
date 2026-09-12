function plot_obstacle_spec
  addpath('../scripts');
  fh = figure('PaperUnits','centimeters','PaperPosition',[0,0,16,8]*1.5);
  vT = 0:0.02:1;
  vD = vT*2-1;
  mIR1 = [];
  for k=1:numel(vT)
    [mIR1(:,k),fs] = render_ir( 'test_pos_obstacle.tsc', vT(k) );
    [mIR2(:,k),fs] = render_ir( 'test_pos_obstacle_inner.tsc', vT(k) );
  end
  mH1 = 20*log10(abs(realfft(mIR1)));
  mH2 = 20*log10(abs(realfft(mIR2)));
  vF = ([1:size(mH1,1)]-1)*10;
  subplot(1,2,1);
  h = pcolor(vF,vD,mH1');
  set(h,'EdgeColor','none');
  xtick = round(1000*2.^[-4:1:2]);
  set(gca,'CLim',[-40,0],'XLim',[25,2500],'XScale','log',...
	  'XTick',xtick,'XTickLabel',num2str(xtick'));
  colorbar();
  xlabel('frequency / Hz');
  ylabel('obstacle center / m');
  title('obstacle');
  subplot(1,2,2);
  h = pcolor(vF,vD,mH2');
  set(h,'EdgeColor','none');
  xtick = round(1000*2.^[-4:1:2]);
  set(gca,'CLim',[-40,0],'XLim',[25,2500],'XScale','log',...
	  'XTick',xtick,'XTickLabel',num2str(xtick'));
  colorbar();
  xlabel('frequency / Hz');
  ylabel('obstacle center / m');
  title('hole');
  drawnow();
  %saveas(fh,'obstacle.eps','epsc');
  saveas(fh,'obstacle.png','png');
  %system('epstopdf obstacle.eps');
end
  
function [ir,fs] = render_ir( fname, t )
  sLib = 'LD_LIBRARY_PATH=../libtascar/build/:../plugins/build/';
  sApp = '../apps/build/tascar_renderir';
  sCmd = sprintf('%s RECTYPE=omni %s -t %g -f 5000 -l 500 -o temp.wav %s',...
		 sLib,sApp,t,fname);
  system(sCmd);
  [ir,fs] = audioread('temp.wav');
  system('rm -f temp.wav');
end
