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
	sed "s/models.[a-zA-Z0-9_]*(/models.$net(/" -i pytorchexample.py
	OMP_NUM_THREADS=8 MKL_NUM_THREADS=8 python3 pytorchexample.py
done
