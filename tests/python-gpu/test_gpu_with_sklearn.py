import itertools
import json
import os
import tempfile
from concurrent.futures import ThreadPoolExecutor

import numpy as np
import pytest

import xgboost as xgb
from xgboost import testing as tm
from xgboost.testing.ranking import run_ranking_categorical, run_ranking_qid_df
from xgboost.testing.with_skl import (
    run_boost_from_prediction_binary,
    run_boost_from_prediction_multi_clasas,
    run_housing_rf_regression,
)

pytestmark = pytest.mark.skipif(**tm.no_sklearn())

rng = np.random.RandomState(1994)


def test_gpu_binary_classification():
    from sklearn.datasets import load_digits
    from sklearn.model_selection import KFold

    digits = load_digits(n_class=2)
    y = digits["target"]
    X = digits["data"]
    kf = KFold(n_splits=2, shuffle=True, random_state=rng)
    for cls in (xgb.XGBClassifier, xgb.XGBRFClassifier):
        for train_index, test_index in kf.split(X, y):
            xgb_model = cls(
                random_state=42,
                tree_method="hist",
                n_estimators=4,
                device="cuda",
            ).fit(X[train_index], y[train_index])
            cfg: str = json.loads(xgb_model.get_booster().save_config())["learner"][
                "generic_param"
            ]["device"]
            assert cfg.startswith("cuda")
            preds = xgb_model.predict(X[test_index])
            labels = y[test_index]
            err = sum(
                1 for i in range(len(preds)) if int(preds[i] > 0.5) != labels[i]
            ) / float(len(preds))
            assert err < 0.1


@pytest.mark.skipif(**tm.no_cupy())
@pytest.mark.skipif(**tm.no_cudf())
@pytest.mark.parametrize("tree_method", ["hist", "approx"])
def test_boost_from_prediction_gpu_hist(tree_method: str) -> None:
    import cudf
    import cupy as cp
    from sklearn.datasets import load_breast_cancer, load_digits

    X, y = load_breast_cancer(return_X_y=True)
    X, y = cp.array(X), cp.array(y)

    run_boost_from_prediction_binary(tree_method, "cuda", X, y, None)
    run_boost_from_prediction_binary(tree_method, "cuda", X, y, cudf.DataFrame)

    X, y = load_digits(return_X_y=True)
    X, y = cp.array(X), cp.array(y)

    run_boost_from_prediction_multi_clasas(
        xgb.XGBClassifier, tree_method, "cuda", X, y, None
    )
    run_boost_from_prediction_multi_clasas(
        xgb.XGBClassifier, tree_method, "cuda", X, y, cudf.DataFrame
    )


def test_num_parallel_tree() -> None:
    run_housing_rf_regression("hist", "cuda")


@pytest.mark.skipif(**tm.no_pandas())
@pytest.mark.skipif(**tm.no_cudf())
@pytest.mark.skipif(**tm.no_sklearn())
def test_categorical():
    import cudf
    import cupy as cp
    import pandas as pd
    from sklearn.datasets import load_svmlight_file

    data_dir = tm.data_dir(__file__)
    X, y = load_svmlight_file(
        os.path.join(data_dir, "agaricus.txt.train"), dtype=np.float32
    )
    clf = xgb.XGBClassifier(
        tree_method="hist",
        device="cuda",
        enable_categorical=True,
        n_estimators=10,
    )
    X = pd.DataFrame(X.todense()).astype("category")
    for c in X.columns:
        X[c] = X[c].cat.rename_categories(int)
    clf.fit(X, y)

    with tempfile.TemporaryDirectory() as tempdir:
        model = os.path.join(tempdir, "categorial.json")
        clf.save_model(model)

        with open(model) as fd:
            categorical = json.load(fd)
            categories_sizes = np.array(
                categorical["learner"]["gradient_booster"]["model"]["trees"][0][
                    "categories_sizes"
                ]
            )
            assert categories_sizes.shape[0] != 0
            np.testing.assert_allclose(categories_sizes, 1)

    def check_predt(X, y):
        reg = xgb.XGBRegressor(
            tree_method="hist", enable_categorical=True, n_estimators=64, device="cuda"
        )
        reg.fit(X, y)
        predts = reg.predict(X)
        booster = reg.get_booster()
        assert "c" in booster.feature_types
        assert len(booster.feature_types) == 1
        inp_predts = booster.inplace_predict(X)
        if isinstance(inp_predts, cp.ndarray):
            inp_predts = cp.asnumpy(inp_predts)
        np.testing.assert_allclose(predts, inp_predts)

    y = [1, 2, 3]
    X = pd.DataFrame({"f0": ["a", "b", "c"]})
    X["f0"] = X["f0"].astype("category")
    check_predt(X, y)

    X = cudf.DataFrame(X)
    check_predt(X, y)


@pytest.mark.skipif(**tm.no_cupy())
@pytest.mark.skipif(**tm.no_cudf())
def test_classififer():
    import cudf
    import cupy as cp
    from sklearn.datasets import load_digits

    X, y = load_digits(return_X_y=True)
    y *= 10

    clf = xgb.XGBClassifier(tree_method="hist", n_estimators=1, device="cuda")

    # numpy
    with pytest.raises(ValueError, match=r"Invalid classes.*"):
        clf.fit(X, y)

    # cupy
    X, y = cp.array(X), cp.array(y)
    with pytest.raises(ValueError, match=r"Invalid classes.*"):
        clf.fit(X, y)

    # cudf
    X, y = cudf.DataFrame(X), cudf.DataFrame(y)
    with pytest.raises(ValueError, match=r"Invalid classes.*"):
        clf.fit(X, y)

    # pandas
    X, y = load_digits(return_X_y=True, as_frame=True)
    y *= 10
    with pytest.raises(ValueError, match=r"Invalid classes.*"):
        clf.fit(X, y)


@pytest.mark.parametrize(
    "use_cupy,tree_method,device,order,gdtype,strategy",
    [
        c
        for c in itertools.product(
            (True, False),
            ("hist", "approx"),
            ("cpu", "cuda"),
            ("C", "F"),
            ("float64", "float32"),
            ("one_output_per_tree", "multi_output_tree"),
        )
    ],
)
def test_custom_objective(
    use_cupy: bool,
    tree_method: str,
    device: str,
    order: str,
    gdtype: str,
    strategy: str,
) -> None:
    from sklearn.datasets import load_iris

    X, y = load_iris(return_X_y=True)

    params = {
        "tree_method": tree_method,
        "device": device,
        "n_estimators": 8,
        "multi_strategy": strategy,
    }

    obj = tm.softprob_obj(y.max() + 1, use_cupy=use_cupy, order=order, gdtype=gdtype)

    clf = xgb.XGBClassifier(objective=obj, **params)

    if strategy == "multi_output_tree" and tree_method == "approx":
        with pytest.raises(ValueError, match=r"Only the hist"):
            clf.fit(X, y)
        return
    if strategy == "multi_output_tree" and device == "cuda":
        with pytest.raises(ValueError, match=r"GPU is not yet"):
            clf.fit(X, y)
        return

    clf.fit(X, y)

    clf_1 = xgb.XGBClassifier(**params)
    clf_1.fit(X, y)

    np.testing.assert_allclose(clf.predict_proba(X), clf_1.predict_proba(X), rtol=1e-4)

    params["n_estimators"] = 2

    def wrong_shape(labels, predt):
        grad, hess = obj(labels, predt)
        return grad[:, :-1], hess[:, :-1]

    with pytest.raises(ValueError, match="should be equal to the number of"):
        clf = xgb.XGBClassifier(objective=wrong_shape, **params)
        clf.fit(X, y)

    def wrong_shape_1(labels, predt):
        grad, hess = obj(labels, predt)
        return grad[:-1, :], hess[:-1, :]

    with pytest.raises(ValueError, match="Mismatched size between the gradient"):
        clf = xgb.XGBClassifier(objective=wrong_shape_1, **params)
        clf.fit(X, y)

    def wrong_shape_2(labels, predt):
        grad, hess = obj(labels, predt)
        return grad[:, :], hess[:-1, :]

    with pytest.raises(ValueError, match="Mismatched shape between the gradient"):
        clf = xgb.XGBClassifier(objective=wrong_shape_2, **params)
        clf.fit(X, y)

    def wrong_shape_3(labels, predt):
        grad, hess = obj(labels, predt)
        grad = grad.reshape(grad.size)
        hess = hess.reshape(hess.size)
        return grad, hess

    with pytest.warns(FutureWarning, match="required to be"):
        clf = xgb.XGBClassifier(objective=wrong_shape_3, **params)
        clf.fit(X, y)


@pytest.mark.skipif(**tm.no_cudf())
def test_ranking_qid_df():
    import cudf

    run_ranking_qid_df(cudf, "hist", "cuda")


@pytest.mark.skipif(**tm.no_pandas())
def test_ranking_categorical() -> None:
    run_ranking_categorical(device="cuda")


@pytest.mark.skipif(**tm.no_cupy())
@pytest.mark.mgpu
def test_device_ordinal() -> None:
    import cupy as cp

    n_devices = 2

    def worker(ordinal: int, correct_ordinal: bool) -> None:
        if correct_ordinal:
            cp.cuda.runtime.setDevice(ordinal)
        else:
            cp.cuda.runtime.setDevice((ordinal + 1) % n_devices)

        X, y, w = tm.make_regression(4096, 12, use_cupy=True)
        reg = xgb.XGBRegressor(device=f"cuda:{ordinal}", tree_method="hist")

        if correct_ordinal:
            reg.fit(
                X, y, sample_weight=w, eval_set=[(X, y)], sample_weight_eval_set=[w]
            )
            assert tm.non_increasing(reg.evals_result()["validation_0"]["rmse"])
            return

        with pytest.raises(ValueError, match="Invalid device ordinal"):
            reg.fit(
                X, y, sample_weight=w, eval_set=[(X, y)], sample_weight_eval_set=[w]
            )

    with ThreadPoolExecutor(max_workers=os.cpu_count()) as executor:
        futures = []
        n_trials = 32
        for i in range(n_trials):
            fut = executor.submit(
                worker, ordinal=i % n_devices, correct_ordinal=i % 3 != 0
            )
            futures.append(fut)

        for fut in futures:
            fut.result()

    cp.cuda.runtime.setDevice(0)
