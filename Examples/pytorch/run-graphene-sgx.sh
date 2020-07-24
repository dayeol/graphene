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
	sed "s/models\.[a-zA-Z0-9_]*(/models\.$net(/" -i pytorchexample.py

	NETFILE=$(ls ~/.cache/torch/checkpoints | grep "$net-")

	if [ -z $NETFILE ]; then
		NETFILE=$(ls ~/.cache/torch/checkpoints | grep "$net")
	fi
	# change manifest
	# echo $NETFILE
	sed "s/[a-z0-9_-]*\.pth$/$NETFILE/" -i pytorch.manifest.template
	make SGX=1 >/dev/null

	SGX=1 OMP_NUM_THREADS=8 MKL_NUM_THREADS=8 ./pal_loader pytorch.manifest pytorchexample.py 2>/dev/null
done
