for net in \
	alexnet \
	squeezenet1_0 \
	vgg19 \
	wide_resnet50_2 \
	resnet50 \
	densenet161 \
	mobilenet_v2\
	googlenet\
	inception_v3
do
	# change python script
	sed "s/models\.[a-zA-Z0-9_]*(/models\.$net(/" -i download-pretrained-model.py

	make clean >> logs
	make download_model >> logs

	LD_LIBRARY_PATH=../ra-tls-secret-prov ../ra-tls-secret-prov/pf_crypt encrypt -w ../ra-tls-secret-prov/files/wrap-key -i ./plaintext/pretrained.pt -o pretrained.pt >> logs
	LD_LIBRARY_PATH=../ra-tls-secret-prov ../ra-tls-secret-prov/pf_crypt encrypt -w ../ra-tls-secret-prov/files/wrap-key -i ./plaintext/input.jpg -o input.jpg >> logs
	LD_LIBRARY_PATH=../ra-tls-secret-prov ../ra-tls-secret-prov/pf_crypt encrypt -w ../ra-tls-secret-prov/files/wrap-key -i ./plaintext/classes.txt -o classes.txt >> logs

	# change manifest
	# echo $NETFILE
	# sed "s/[a-z0-9_-]*\.pth$/$NETFILE/" -i pytorch.manifest.template
	make SGX=1 >/dev/null

	SGX=1 OMP_NUM_THREADS=8 MKL_NUM_THREADS=8  ./pal_loader pytorch.manifest pytorchexample.py 2>> logs
done

