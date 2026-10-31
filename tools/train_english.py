#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Train a hashed character-ngram logistic classifier. NumPy is training-only.

Inputs are a licensed Hunspell .dic (base words only; affixes are not expanded)
and TSV from `go run ./tools/vietnamese-corpus`. No user typing is collected.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np

DIM = 4096
MAX_WORD = 32
WIDTH = 3 * (MAX_WORD + 2) - 6


def features(word):
    word = '^' + word + '$'
    indices = []
    for n in (2, 3, 4):
        for start in range(len(word) - n + 1):
            value = 2166136261
            for char in word[start:start+n].encode('ascii'):
                value = ((value ^ char) * 16777619) & 0xffffffff
            indices.append(value % DIM)
    return indices


def eligible(word):
    return 4 <= len(word) <= MAX_WORD and word.isascii() and word.isalpha()


def split(group):
    # Whole syllable families stay together across tone variants.
    return int.from_bytes(hashlib.sha256(group.encode()).digest()[:4], 'little') % 10


def sigmoid(value):
    return 1 / (1 + np.exp(-np.clip(value, -40, 40)))


def metrics(scores, labels, threshold):
    en = labels == 1
    return {
        'english_examples': int(en.sum()), 'vietnamese_examples': int((~en).sum()),
        'english_detection_rate': float((scores[en] >= threshold).mean()),
        'vietnamese_false_english_rate': float((scores[~en] >= threshold).mean()),
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--english', type=Path, required=True)
    parser.add_argument('--vietnamese', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    assert features('test')[:5] == [2351, 3762, 3433, 578, 2949], 'feature hash changed'
    english = {line.split('/')[0].split()[0].lower()
               for line in args.english.read_text().splitlines()[1:] if line.strip()}
    english = {word for word in english if eligible(word)}
    vietnamese = {}
    for line in args.vietnamese.read_text().splitlines():
        group, word = line.split('\t')
        if eligible(word):
            vietnamese.setdefault(word, group)
    overlap = english & vietnamese.keys()
    records = [(w, 1, 'en:' + w) for w in sorted(english - overlap)]
    records += [(w, 0, 'vi:' + vietnamese[w]) for w in sorted(vietnamese.keys() - overlap)]
    words, labels, groups = zip(*records)
    y = np.array(labels, dtype=np.float64)
    x = np.full((len(words), WIDTH), DIM, dtype=np.int32)
    scale = np.empty(len(words))
    for row, word in enumerate(words):
        indices = features(word)
        x[row, :len(indices)] = indices
        scale[row] = 1 / np.sqrt(len(indices))
    folds = np.array([split(g) for g in groups])
    train, valid, test = folds >= 2, folds == 1, folds == 0
    assert min(train.sum(), valid.sum(), test.sum()) > 1000
    class_weights = np.where(y == 1, train.sum() / (2 * y[train].sum()),
                            train.sum() / (2 * (1-y[train]).sum()))
    weights = np.zeros(DIM+1)
    bias = 0.0
    rng = np.random.default_rng(7)
    train_rows = np.flatnonzero(train)
    for epoch in range(30):
        rng.shuffle(train_rows)
        rate = 0.8 / (1 + epoch * 0.05)
        for start in range(0, len(train_rows), 512):
            rows = train_rows[start:start+512]
            error = (sigmoid(weights[x[rows]].sum(axis=1) * scale[rows] + bias) - y[rows]) * class_weights[rows]
            grad = np.bincount(x[rows].ravel(), weights=np.repeat(error * scale[rows], WIDTH), minlength=DIM+1) / len(rows)
            weights[:DIM] -= rate * (grad[:DIM] + 0.0002 * weights[:DIM])
            bias -= rate * error.mean()
    # Export float32, then evaluate precisely the exported weights.
    weights = weights.astype(np.float32)
    bias = float(np.float32(bias))
    scores = sigmoid(weights[x].sum(axis=1, dtype=np.float64) * scale + bias)
    vn_valid = scores[valid & (y == 0)]
    threshold = max(0.8, float(np.quantile(vn_valid, 0.999, method='higher')) + 1e-7)
    threshold = float(np.float32(min(threshold, 1.0)))
    report = {
        'model': 'hashed character 2/3/4-gram logistic regression',
        'seed': 7, 'dimensions': DIM, 'word_length': [4, MAX_WORD],
        'score_is_calibrated_intent_probability': False,
        'training_examples': int(train.sum()), 'excluded_ambiguous_spellings': len(overlap),
        'threshold': threshold,
        'validation': metrics(scores[valid], y[valid], threshold),
        'test': metrics(scores[test], y[test], threshold),
        'english_sha256': hashlib.sha256(args.english.read_bytes()).hexdigest(),
        'vietnamese_sha256': hashlib.sha256(args.vietnamese.read_bytes()).hexdigest(),
        'probes': {},
    }
    for word in ['typescript', 'javascript', 'webpack', 'localhost', 'kubernetes', 'nginx', 'access', 'vieetj', 'tieengs', 'nguwowif', 'ddaays', 'hoaf']:
        indices = features(word)
        report['probes'][word] = float(sigmoid(weights[indices].sum(dtype=np.float64) / np.sqrt(len(indices)) + bias))
    assert report['test']['vietnamese_false_english_rate'] < 0.005, 'unsafe held-out false positive rate'
    assert report['test']['english_detection_rate'] > 0.75, 'insufficient English detection'
    args.output.mkdir(parents=True, exist_ok=True)
    artifact = struct.pack('<ff', bias, threshold) + weights[:DIM].astype('<f4').tobytes()
    (args.output / 'english-v1.bin').write_bytes(artifact)
    report['model_sha256'] = hashlib.sha256(artifact).hexdigest()
    (args.output / 'english-v1.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps(report, indent=2))


if __name__ == '__main__':
    main()
