#!/bin/bash

set -e

# define the number of threads you want to run
NUM_THREADS=8
SCRIPT=pytorchexample.py

# set the filename you want stderr to be forwarded
STDERRFWD=/dev/null

networks=(
	alexnet
	squeezenet1_0
	vgg19
	wide_resnet50_2
	resnet50
	densenet161
	mobilenet_v2
	googlenet
	inception_v3
)

if [ "$#" -eq  "0" ]
then
	echo "this script needs an argument"
	echo "choose one of followings: native, graphene, sgx, ppml"
	exit 1
fi

if [ "$1" == "ppml" ]
then
	cp pytorch.manifest.template.ppml pytorch.manifest.template
	sed "s/^#alexnet/alexnet/" -i pytorchexample.py
	sed "s/^alexnet = models/#alexnet = models/" -i pytorchexample.py
else
	cp plaintext/* .
	cp pytorch.manifest.template.sgx pytorch.manifest.template
	sed "s/^#alexnet/alexnet/" -i pytorchexample.py
	sed "s/^alexnet = torch/#alexnet = torch/" -i pytorchexample.py
fi


for net in "${networks[@]}"
do
	echo "=== $net ==="

	# set the number of threads
	sed "s/set_num_threads([0-9]*)/set_num_threads($NUM_THREADS)/" -i $SCRIPT

	# change the python script to switch the model
	sed "s/models.[a-zA-Z0-9_]*(/models.$net(/" -i $SCRIPT

  ###########################
  #         Native          #
  ###########################
	if [ "$1" == "native" ]
	then
		OMP_NUM_THREADS=$NUM_THREADS python3 $SCRIPT
	fi

  ###########################
  #     Graphene w/o SGX    #
  ###########################
	if [ "$1" == "graphene" ]
	then
		OMP_NUM_THREADS=$NUM_THREADS ./pal_loader pytorch.manifest $SCRIPT 2>$STDERRFWD
	fi

  ###########################
  #     Graphene w/ SGX     #
  ###########################
	if [ "$1" == "sgx" ]
	then
		NETFILE=$(ls ~/.cache/torch/checkpoints | grep "$net-")
		if [ -z $NETFILE ]; then
			NETFILE=$(ls ~/.cache/torch/checkpoints | grep "$net")
		fi
		sed "s/[a-z0-9_-]*\.pth$/$NETFILE/" -i pytorch.manifest.template
		make SGX=1 >/dev/null
		SGX=1 OMP_NUM_THREADS=$NUM_THREADS ./pal_loader pytorch.manifest $SCRIPT 2>$STDERRFWD
	fi

  ###########################
  #     Graphene w/ SGX     #
  ###########################
	if [ "$1" == "ppml" ]
	then
		sed "s/models\.[a-zA-Z0-9_]*(/models\.$net(/" -i download-pretrained-model.py
		make clean >/dev/null
		make download_model >/dev/null
		LD_LIBRARY_PATH=../ra-tls-secret-prov ../ra-tls-secret-prov/pf_crypt encrypt -w ../ra-tls-secret-prov/files/wrap-key -i ./plaintext/pretrained.pt -o pretrained.pt >/dev/null
		LD_LIBRARY_PATH=../ra-tls-secret-prov ../ra-tls-secret-prov/pf_crypt encrypt -w ../ra-tls-secret-prov/files/wrap-key -i ./plaintext/input.jpg -o input.jpg >/dev/null
		LD_LIBRARY_PATH=../ra-tls-secret-prov ../ra-tls-secret-prov/pf_crypt encrypt -w ../ra-tls-secret-prov/files/wrap-key -i ./plaintext/classes.txt -o classes.txt >/dev/null
		make SGX=1 >/dev/null
		SGX=1 OMP_NUM_THREADS=$NUM_THREADS ./pal_loader pytorch.manifest $SCRIPT 2>$STDERRFWD
	fi
done
