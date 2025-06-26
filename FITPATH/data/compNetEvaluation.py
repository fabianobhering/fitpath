import sys, numpy, scipy.stats

nSource=12
nInst=500*nSource

refNS3= "/local/cluster04/fabianobhering/ns-3/ns-3-dev/results/NS3VBRGrid"+str(nSource)+"/netEvalComp.txt"
refEvalvid= "/local/cluster04/fabianobhering/ns-3/ns-3-dev/results/NS3VBRGrid"+str(nSource)+"/netEvalutation.txt"


refAFTER="estimates/EstimatesAFTER_Grid"+str(nSource)#"/local/cluster05/fabianobhering/ns-3/ns-3-dev/results/NS3VBRRand3/netEvalutation.txt"

refAFTER_MFR="estimates/EstimatesAFTER-MFR_Grid"+str(nSource)

refInst="instances/instGrid"+str(nSource)




#THROUGHPUT

#Compara AFTER com AFTER-MFR
# = open("compNetEvalRand3","w")
ssim_instA =[]
ssim_instB =[]
rate_instA =[]
rate_instB =[]
rate_after =[]
rate_after_mfr =[]


inv_after=0
inv_after_mfr=0

ratio_after=[]
ratio_after_mfr=[]

flow_instA =[]
flow_instB =[]

srcA =[]
srcB =[]
destA =[]
destB =[]

c=0
m_afterInversion =[]
m_after_mfrInversion=[]
tie=0

ref_arquivo = open(refEvalvid,"r")
		
	
for linha in ref_arquivo:
	valores = linha.split()
	ssim_instA.append(round(float(valores[0])))
	ssim_instB.append(round(float(valores[0])))
ref_arquivo.close()
		

ref_arquivo = open(refNS3,"r")
		
		
for linha in ref_arquivo:
	valores = linha.split()
	rate_instA.append(round(float(valores[2])))
	rate_instB.append(round(float(valores[2])))
	flow_instA.append(int(valores[1]))
	flow_instB.append(int(valores[1]))	
ref_arquivo.close()
		

ref_arquivo = open(refAFTER,"r")
		
		
for linha in ref_arquivo:
	valores = linha.split()
	rate_after.append(round(float(valores[0])))
		
ref_arquivo.close()

ref_arquivo = open(refAFTER_MFR,"r")
	
for linha in ref_arquivo:
	valores = linha.split()
	rate_after_mfr.append(round(float(valores[0])))
	
ref_arquivo.close()
		
ref_arquivo = open(refInst,"r")
	
for linha in ref_arquivo:
	valores = linha.split()
	srcA.append(int(valores[0]))
	srcB.append(int(valores[0]))
	destA.append(int(valores[1]))
	destB.append(int(valores[1]))
	
ref_arquivo.close()



for instA in range (0,nInst):
	#print flow_instA[instA],ssim_instA[instA]
	for instB in range (0,nInst):
		
		if (flow_instA[instA]==flow_instB[instB] or flow_instA[instA]+3==flow_instB[instB]) and instA<>instB:# and (ssim_instA[instA]>100 or ssim_instB[instB]<100):# and (srcA[instA]==srcB[instB] and destA[instA]==destB[instB]):	
			
			#print rate_instA[instA],rate_instB[instB], rate_after[instA],rate_after[instB], rate_after_mfr[instA],rate_after_mfr[instB]
			#print flow_instA[instA], flow_instB[instB]
			
			c=c+1
			
			
			
			
			if  (ssim_instA[instA]>ssim_instB[instB] and rate_after[instA]<rate_after[instB]) or (ssim_instA[instA]<ssim_instB[instB] and rate_after[instA]>rate_after[instB]):# or (ssim_instA[instA]==ssim_instB[instB] and rate_after[instA]<>rate_after[instB]) or (ssim_instA[instA]<>ssim_instB[instB] and rate_after[instA]==rate_after[instB]):
				#if((ssim_instA[instA]-ssim_instB[instB] > 1 or ssim_instA[instA]-ssim_instB[instB] < -1) and (rate_after[instA]-rate_after[instB] > 1 or rate_after[instA]-rate_after[instB]< -1)):
				
				#print "inversion after"
				
				inv_after=inv_after+1
				m_afterInversion.append(abs(ssim_instA[instA]-ssim_instB[instB]))
				
					#print ssim_instA[instA],ssim_instB[instB], rate_after[instA],rate_after[instB]
			
			else:
				
				#print "ratio after", rate_instA[instA]/rate_after[instA] , rate_instB[instB]/rate_after[instB]
				if(rate_after[instA]==0 and rate_instA[instA]==0):
					ratio_after.append(1.0)
				else:
					if(rate_instA[instA]>0):
						ratio_after.append(rate_after[instA]/rate_instA[instA])
				
				if(rate_after[instB]==0 and rate_instA[instB]==0):
					ratio_after.append(1.0)
				else:
					if(rate_instA[instB]>0):
						ratio_after.append(rate_after[instB]/rate_instA[instB])
			
			
			
			
			if(rate_after_mfr[instA]==rate_after_mfr[instB]):
				
				tie = tie+1;
				
			if  (ssim_instA[instA]>ssim_instB[instB] and rate_after_mfr[instA]<rate_after_mfr[instB]) or (ssim_instA[instA]<ssim_instB[instB] and rate_after_mfr[instA]>rate_after_mfr[instB]):# or (ssim_instA[instA]==ssim_instB[instB] and rate_after_mfr[instA]<>rate_after_mfr[instB]) or (ssim_instA[instA]<>ssim_instB[instB] and rate_after_mfr[instA]==rate_after_mfr[instB]):
				
				#if((ssim_instA[instA]-ssim_instB[instB] > 1 or ssim_instA[instA]-ssim_instB[instB] < -1) and (rate_after_mfr[instA]-rate_after_mfr[instB] > 1 or rate_after_mfr[instA]-rate_after_mfr[instB]< -1)):
					#print "inversion after-mfr ", abs(ssim_instA[instA]-ssim_instB[instB])
				
				inv_after_mfr=inv_after_mfr+1
				
				m_after_mfrInversion.append(abs(ssim_instA[instA]-ssim_instB[instB]))
					
				#if abs(rate_after_mfr[instA]-rate_after_mfr[instB])>20: 
				#	print instA+3, "	",instB+3, rate_after_mfr[instA]-rate_after_mfr[instB], ssim_instA[instA]-ssim_instB[instB]
					
					#print srcA[instA],  destA[instA], srcB[instB],  destB[instB]
					#print rate_after_mfr[instA],rate_after_mfr[instB], ssim_instA[instA],ssim_instB[instB]
			
			else:
					
				#print "ratio after-mfr", rate_instA[instA]/rate_after_mfr[instA] , rate_instA[instB]/rate_after_mfr[instB]
				if(rate_after_mfr[instA]==0 and rate_instA[instA]==0):
					ratio_after_mfr.append(1.0)
				else:
					if(rate_instA[instA]>0):
						ratio_after_mfr.append(rate_after_mfr[instA]/rate_instA[instA])
				
				if(rate_after_mfr[instB]==0 and rate_instA[instB]==0):
					ratio_after_mfr.append(1.0)
				else:
					if(rate_instA[instB]>0):
						ratio_after_mfr.append(rate_after_mfr[instB]/rate_instA[instB])
		
			
			

print tie, c,round(float(tie)/c,2)
print "inversion:", round(float(inv_after)/c,2), round(float(inv_after_mfr)/c,2)
#print "ratio:", round(numpy.mean(ratio_after),2), round(numpy.std(ratio_after),2) , round(numpy.mean(ratio_after_mfr),2), round(numpy.std(ratio_after_mfr),2)
#print "correlation:", round(scipy.stats.pearsonr(rate_after, rate_instA)[0],2), round(scipy.stats.pearsonr(rate_after_mfr, rate_instA)[0],2)
#print "THROUGHPUT","	",round(float(inv_after)/c,3),"	",round(float(inv_after_mfr)/c,3),"	",round(numpy.mean(ratio_after),2),"	",round(numpy.std(ratio_after),2),"	",round(numpy.mean(ratio_after_mfr),2),"	",round(numpy.std(ratio_after_mfr),2),"	", round(scipy.stats.pearsonr(rate_after, ssim_instA)[0],2),"	", round(scipy.stats.pearsonr(rate_after_mfr, ssim_instA)[0],2)

#print "meanInversion:", float(m_afterInversion/c_afterInversion)
#print round(float(inv_after)/c,3),"	",round(numpy.mean(m_afterInversion),2),"	",round(numpy.std(m_afterInversion),2),"	", round(float(inv_after_mfr)/c,3),"	",round(numpy.mean(m_after_mfrInversion),2),"	",round(numpy.std(m_after_mfrInversion),2),"	" 

#print c, round(float(inv_after)/c,3),"	",round(numpy.mean(m_afterInversion),2),"	", round(float(inv_after_mfr)/c,3),"	",round(numpy.mean(m_after_mfrInversion),2)
exit()

#print "correlation ssim:", scipy.stats.pearsonr(rate_after, ssim_instA), scipy.stats.pearsonr(rate_after_mfr, ssim_instA)
		#f.write(str(int(100*sumVal/c))+"\n")
			


#PACKET LOSS

#Compara AFTER com AFTER-MFR
# = open("compNetEvalRand3","w")
ssim_instA =[]
ssim_instB =[]
rate_instA =[]
rate_instB =[]
rate_after =[]
rate_after_mfr =[]


inv_after=0
inv_after_mfr=0

ratio_after=[]
ratio_after_mfr=[]

flow_instA =[]
flow_instB =[]

c=0



ref_arquivo = open(refEvalvid,"r")
		
	
for linha in ref_arquivo:
	valores = linha.split()
	ssim_instA.append(round(float(valores[3])))
	ssim_instB.append(round(float(valores[3])))
ref_arquivo.close()
		

ref_arquivo = open(refNS3,"r")
		
		
for linha in ref_arquivo:
	valores = linha.split()
	rate_instA.append(round(float(valores[3])))
	rate_instB.append(round(float(valores[3])))
	flow_instA.append(int(valores[1]))
	flow_instB.append(int(valores[1])) 
ref_arquivo.close()
		

ref_arquivo = open(refAFTER,"r")
		
		
for linha in ref_arquivo:
	valores = linha.split()
	rate_after.append(round(float(valores[1])))
		
ref_arquivo.close()

ref_arquivo = open(refAFTER_MFR,"r")
		
	
for linha in ref_arquivo:
	valores = linha.split()
	rate_after_mfr.append(round(float(valores[1])))
	
ref_arquivo.close()
		

for instA in range (0,nInst):
	#print flow_instA[instA],ssim_instA[instA]
	for instB in range (0,nInst):
		
		if (flow_instA[instA]==flow_instB[instB] or flow_instA[instA]+3==flow_instB[instB])  and instA<>instB:#and (ssim_instA[instA]<100 or ssim_instB[instB]<100):	
			#print rate_instA[instA],rate_instB[instB], rate_after[instA],rate_after[instB], rate_after_mfr[instA],rate_after_mfr[instB]
			#print flow_instA[instA], flow_instB[instB]
			
			c=c+1
			if  (ssim_instA[instA]>ssim_instB[instB] and rate_after[instA]<rate_after[instB]) or (ssim_instA[instA]<ssim_instB[instB] and rate_after[instA]>rate_after[instB]):# or (ssim_instA[instA]==ssim_instB[instB] and rate_after[instA]<>rate_after[instB]) or (ssim_instA[instA]<>ssim_instB[instB] and rate_after[instA]==rate_after[instB]):
				#print "inversion after"
				
				inv_after=inv_after+1
				#print ssim_instA[instA],ssim_instB[instB], rate_after[instA],rate_after[instB]
			
			else:
				
				#print "ratio after", rate_instA[instA]/rate_after[instA] , rate_instB[instB]/rate_after[instB]
				if(rate_after[instA]==0 and rate_instA[instA]==0):
					ratio_after.append(1.0)
				else:
					if(rate_instA[instA]>0):
						ratio_after.append(rate_after[instA]/rate_instA[instA])
				
				if(rate_after[instB]==0 and rate_instA[instB]==0):
					ratio_after.append(1.0)
				else:
					if(rate_instA[instB]>0):
						ratio_after.append(rate_after[instB]/rate_instA[instB])
			
			
			if  (ssim_instA[instA]>ssim_instB[instB] and rate_after_mfr[instA]<rate_after_mfr[instB]) or (ssim_instA[instA]<ssim_instB[instB] and rate_after_mfr[instA]>rate_after_mfr[instB]):# or (ssim_instA[instA]==ssim_instB[instB] and rate_after_mfr[instA]<>rate_after_mfr[instB]) or (ssim_instA[instA]<>ssim_instB[instB] and rate_after_mfr[instA]==rate_after_mfr[instB]):
				
				#if((ssim_instA[instA]-ssim_instB[instB] > 1 or ssim_instA[instA]-ssim_instB[instB] < -1) and (rate_after_mfr[instA]-rate_after_mfr[instB] > 1 or rate_after_mfr[instA]-rate_after_mfr[instB]< -1)):
					#print "inversion after-mfr"
				inv_after_mfr=inv_after_mfr+1
					#print ssim_instA[instA],ssim_instB[instB], rate_after_mfr[instA],rate_after_mfr[instB]
			
			else:
					
				#print "ratio after-mfr", rate_instA[instA]/rate_after_mfr[instA] , rate_instA[instB]/rate_after_mfr[instB]
				if(rate_after_mfr[instA]==0 and rate_instA[instA]==0):
					ratio_after_mfr.append(1.0)
				else:
					if(rate_instA[instA]>0):
						ratio_after_mfr.append(rate_after_mfr[instA]/rate_instA[instA])
				
				if(rate_after_mfr[instB]==0 and rate_instA[instB]==0):
					ratio_after_mfr.append(1.0)
				else:
					if(rate_instA[instB]>0):
						ratio_after_mfr.append(rate_after_mfr[instB]/rate_instA[instB])
			


#print "inversion:", round(float(inv_after)/c,2), round(float(inv_after_mfr)/c,2)
#print "ratio:", round(numpy.mean(ratio_after),2), round(numpy.std(ratio_after),2) , round(numpy.mean(ratio_after_mfr),2), round(numpy.std(ratio_after_mfr),2)
#print "correlation:", scipy.stats.pearsonr(rate_after, rate_instA), scipy.stats.pearsonr(rate_after_mfr, rate_instA)

print "PACKET LOSS","	",round(float(inv_after)/c,2),"	",round(float(inv_after_mfr)/c,2),"	",round(numpy.mean(ratio_after),2),"	",round(numpy.std(ratio_after),2),"	",round(numpy.mean(ratio_after_mfr),2),"	",round(numpy.std(ratio_after_mfr),2),"	", round(scipy.stats.pearsonr(rate_after, ssim_instA)[0],2),"	", round(scipy.stats.pearsonr(rate_after_mfr, ssim_instA)[0],2)
#print "correlation ssim:", scipy.stats.pearsonr(rate_after, ssim_instA), scipy.stats.pearsonr(rate_after_mfr, ssim_instA)
		#f.write(str(int(100*sumVal/c))+"\n")
	


#DELAY

#Compara AFTER com AFTER-MFR
# = open("compNetEvalRand3","w")
ssim_instA =[]
ssim_instB =[]
rate_instA =[]
rate_instB =[]
rate_after =[]
rate_after_mfr =[]


inv_after=0
inv_after_mfr=0

ratio_after=[]
ratio_after_mfr=[]

flow_instA =[]
flow_instB =[]

c=0



ref_arquivo = open(refEvalvid,"r")
		
	
for linha in ref_arquivo:
	valores = linha.split()
	ssim_instA.append(round(float(valores[3])))
	ssim_instB.append(round(float(valores[3])))
ref_arquivo.close()
		

ref_arquivo = open(refNS3,"r")
		
		
for linha in ref_arquivo:
	valores = linha.split()
	rate_instA.append(round(float(valores[4])))
	rate_instB.append(round(float(valores[4])))
	flow_instA.append(int(valores[1]))
	flow_instB.append(int(valores[1]))	
ref_arquivo.close()
		

ref_arquivo = open(refAFTER,"r")
		
		
for linha in ref_arquivo:
	valores = linha.split()
	rate_after.append(round(float(valores[2])))
		
ref_arquivo.close()

ref_arquivo = open(refAFTER_MFR,"r")
		
	
for linha in ref_arquivo:
	valores = linha.split()
	rate_after_mfr.append(round(float(valores[2])))
	
ref_arquivo.close()
		

for instA in range (0,nInst):
	#print flow_instA[instA],rate_instA[instA], rate_after[instA], rate_after_mfr[instA]
	for instB in range (0,nInst):
		
		if (flow_instA[instA]==flow_instB[instB] or flow_instA[instA]+3==flow_instB[instB])  and instA<>instB and rate_after[instA]>0 and rate_after_mfr[instA]>0 and rate_after[instB]>0 and rate_after_mfr[instB]>0:#and (ssim_instA[instA]<100 or ssim_instB[instB]<100):	
			#print rate_instA[instA],rate_instB[instB], rate_after[instA],rate_after[instB], rate_after_mfr[instA],rate_after_mfr[instB]
			#print flow_instA[instA], flow_instB[instB]
			
			c=c+1
			if  (ssim_instA[instA]>ssim_instB[instB] and rate_after[instA]<rate_after[instB]) or (ssim_instA[instA]<ssim_instB[instB] and rate_after[instA]>rate_after[instB]):# or (ssim_instA[instA]==ssim_instB[instB] and rate_after[instA]<>rate_after[instB]) or (ssim_instA[instA]<>ssim_instB[instB] and rate_after[instA]==rate_after[instB]):
				#print "inversion after"
				
				inv_after=inv_after+1
				#print ssim_instA[instA],ssim_instB[instB], rate_after[instA],rate_after[instB]
			
			else:
				
				#print "ratio after", rate_instA[instA]/rate_after[instA] , rate_instB[instB]/rate_after[instB]
				if(rate_after[instA]==0 and rate_instA[instA]==0):
					ratio_after.append(1.0)
				else:
					if(rate_instA[instA]>0):
						ratio_after.append(rate_after[instA]/rate_instA[instA])
				
				if(rate_after[instB]==0 and rate_instA[instB]==0):
					ratio_after.append(1.0)
				else:
					if(rate_instA[instB]>0):
						ratio_after.append(rate_after[instB]/rate_instA[instB])
			
			
			if  (ssim_instA[instA]>ssim_instB[instB] and rate_after_mfr[instA]<rate_after_mfr[instB]) or (ssim_instA[instA]<ssim_instB[instB] and rate_after_mfr[instA]>rate_after_mfr[instB]):# or (ssim_instA[instA]==ssim_instB[instB] and rate_after_mfr[instA]<>rate_after_mfr[instB]) or (ssim_instA[instA]<>ssim_instB[instB] and rate_after_mfr[instA]==rate_after_mfr[instB]):
				
				#if((ssim_instA[instA]-ssim_instB[instB] > 1 or ssim_instA[instA]-ssim_instB[instB] < -1) and (rate_after_mfr[instA]-rate_after_mfr[instB] > 1 or rate_after_mfr[instA]-rate_after_mfr[instB]< -1)):
					#print "inversion after-mfr"
				inv_after_mfr=inv_after_mfr+1
					#print ssim_instA[instA],ssim_instB[instB], rate_after_mfr[instA],rate_after_mfr[instB]
			
			else:
					
				#print "ratio after-mfr", rate_instA[instA]/rate_after_mfr[instA] , rate_instA[instB]/rate_after_mfr[instB]
				if(rate_after_mfr[instA]==0 and rate_instA[instA]==0):
					ratio_after_mfr.append(1.0)
				else:
					if(rate_instA[instA]>0):
						ratio_after_mfr.append(rate_after_mfr[instA]/rate_instA[instA])
				
				if(rate_after_mfr[instB]==0 and rate_instA[instB]==0):
					ratio_after_mfr.append(1.0)
				else:
					if(rate_instA[instB]>0):
						ratio_after_mfr.append(rate_after_mfr[instB]/rate_instA[instB])
			

	
#print "inversion:", round(float(inv_after)/c,2), round(float(inv_after_mfr)/c,2)
#print "ratio:", round(numpy.mean(ratio_after),2), round(numpy.std(ratio_after),2) , round(numpy.mean(ratio_after_mfr),2), round(numpy.std(ratio_after_mfr),2)
#print "correlation:", round(scipy.stats.pearsonr(rate_after, rate_instA)[0],2), round(scipy.stats.pearsonr(rate_after_mfr, rate_instA)[0],2)

print "DELAY","	",round(float(inv_after)/c,2),"	",round(float(inv_after_mfr)/c,2),"	",round(numpy.mean(ratio_after),2),"	",round(numpy.std(ratio_after),2),"	",round(numpy.mean(ratio_after_mfr),2),"	",round(numpy.std(ratio_after_mfr),2),"	", round(scipy.stats.pearsonr(rate_after, ssim_instA)[0],2),"	", round(scipy.stats.pearsonr(rate_after_mfr, ssim_instA)[0],2)
#print "correlation ssim:", scipy.stats.pearsonr(rate_after, ssim_instA), scipy.stats.pearsonr(rate_after_mfr, ssim_instA)
		#f.write(str(int(100*sumVal/c))+"\n")
	

