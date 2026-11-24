import torch

if not torch.version.hip:
    raise RuntimeError("Install a Windows ROCm build of PyTorch")

matrix = torch.ones((32, 32), dtype=torch.bfloat16, device="cuda")
result = (matrix @ matrix).cpu()
torch.testing.assert_close(result, torch.full_like(result, 32), rtol=0, atol=0)
print(f"{torch.cuda.get_device_name(0)}: BF16 matrix multiplication passed")
