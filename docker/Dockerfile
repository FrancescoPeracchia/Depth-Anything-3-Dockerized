# syntax=docker/dockerfile:1

ARG CUDA_TAG=12.8.1-cudnn-runtime-ubuntu22.04
FROM nvidia/cuda:${CUDA_TAG} AS runtime

ARG DEBIAN_FRONTEND=noninteractive

# Torch CUDA wheels selector (default: cu128)
ARG TORCH_INDEX_URL=https://download.pytorch.org/whl/cu128

WORKDIR /app

RUN apt-get update \
  && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-venv \
    git ca-certificates curl \
    libgl1 libglib2.0-0 \
  && rm -rf /var/lib/apt/lists/*

RUN python3 -m pip install --no-cache-dir -U pip setuptools wheel

# Install torch first to ensure CUDA-matched wheels
RUN python3 -m pip install --no-cache-dir --index-url ${TORCH_INDEX_URL} \
    torch torchvision torchaudio

# Project deps
COPY requirements.txt /app/requirements.txt
RUN python3 -m pip install --no-cache-dir -r /app/requirements.txt

# Install package (provides `da3` CLI)
COPY README.md /app/README.md
COPY pyproject.toml /app/pyproject.toml
COPY src /app/src
RUN python3 -m pip install --no-cache-dir /app

# Backend defaults
ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PYTORCH_CUDA_ALLOC_CONF=expandable_segments:True,max_split_size_mb:128 \
    DA3_MODEL_DIR=depth-anything/DA3NESTED-GIANT-LARGE \
    DA3_DEVICE=cuda \
    DA3_HOST=0.0.0.0 \
    DA3_PORT=8000

COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 8000

ENTRYPOINT ["/entrypoint.sh"]
