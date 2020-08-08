from torchvision import models
import torch

output_filename = "plaintext/pretrained.pt"
alexnet = models.alexnet(pretrained=True)
torch.save(alexnet, output_filename)

print("Pre-trained model was saved in \"%s\"" % output_filename)
