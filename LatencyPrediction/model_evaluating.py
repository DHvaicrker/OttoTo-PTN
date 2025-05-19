import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
from catboost import CatBoostRegressor
from sklearn.inspection import permutation_importance
from xgboost import XGBRegressor
from sklearn.ensemble import HistGradientBoostingRegressor
from sklearn.model_selection import cross_val_score, KFold
from sklearn.metrics import make_scorer, r2_score, mean_absolute_error, mean_squared_error



def load_data(path):
    """Loads dataset and splits into features and target."""
    df = pd.read_csv(path, encoding="utf-8-sig")
    X = df.drop(columns=['total_delay_min'])
    y = df['total_delay_min']
    return X, y


def evaluate_cv(model, X, y, cv=5):
    """Performs cross-validation and returns dict of mean metrics."""
    kf = KFold(n_splits=cv, shuffle=True, random_state=42)
    scoring = {
        'R2': make_scorer(r2_score),
        'MAE': make_scorer(mean_absolute_error),
        'MSE': make_scorer(mean_squared_error)
    }
    scores = {name: cross_val_score(model, X, y, cv=kf, scoring=scorer)
              for name, scorer in scoring.items()}
    # Compute RMSE from MSE
    mean_scores = {f: np.mean(vals) for f, vals in scores.items()}
    mean_scores['RMSE'] = np.sqrt(mean_scores['MSE'])
    return mean_scores


def plot_metrics_comparison(results):
    """Plots a grouped bar chart of metrics for each model with annotations."""
    df = pd.DataFrame(results).set_index('Model')[['R2', 'MAE', 'MSE', 'RMSE']]
    ax = df.plot(kind='bar', figsize=(10, 6))
    plt.title('5-Fold CV Model Metrics Comparison')
    plt.xlabel('Model')
    plt.ylabel('Score')
    plt.xticks(rotation=0)
    plt.legend(title='Metric')
    plt.grid(axis='y', linestyle='--', alpha=0.7)

    # Annotate each bar with its value
    for container in ax.containers:
        ax.bar_label(container, fmt='%.2f', padding=3)

    plt.tight_layout()
    plt.show()

def plot_top_features(model, feature_names, title, X, y, top_n=15):
    """Universal bar chart of top feature importances."""
    try:
        # For CatBoost
        if isinstance(model, CatBoostRegressor):
            model.fit(X, y, verbose=0)  # Ensure the model is trained
            imp = dict(zip(feature_names, model.get_feature_importance()))
        # For XGBoost
        elif isinstance(model, XGBRegressor):
            model.fit(X, y)  # Ensure the model is trained
            importance = model.feature_importances_
            # If using feature_importances_, the output is an array, map to feature names
            imp = dict(zip(feature_names, importance))
        # For HistGradientBoosting
        elif isinstance(model, HistGradientBoostingRegressor):
            model.fit(X, y)  # Ensure the model is trained
            result = permutation_importance(model, X, y, n_repeats=10, random_state=42)
            imp = dict(zip(feature_names, result.importances_mean))
        else:
            raise ValueError("Unsupported model type.")
    except Exception as e:
        print(f"Error retrieving feature importances: {e}")
        return

    # Ensure the importance dictionary is not empty
    if not imp:
        print("Feature importance data is empty.")
        return

    # Plot the top N features
    df_imp = pd.Series(imp).nlargest(top_n)
    plt.figure(figsize=(12, 8))  # Increase the figure size
    df_imp.plot(kind='bar')
    plt.title(title)
    plt.xticks(rotation=45, ha='right')  # Rotate labels diagonally
    plt.ylabel('Importance')
    plt.tight_layout()
    plt.show()

def main():
    # Load data
    X, y = load_data(r"data/Tel-Aviv_model_samples.csv")
    results = []

    # CatBoost
    cb = CatBoostRegressor(iterations=1000, learning_rate=0.1,
                           depth=10, l2_leaf_reg=3,
                           random_seed=42, verbose=0)
    plot_top_features(cb, X.columns, f"Top Features (CatBoost)", X, y)
    metrics_cb = evaluate_cv(cb, X, y)
    metrics_cb['Model'] = 'CatBoost'
    results.append(metrics_cb)

    # XGBoost
    xgb = XGBRegressor(colsample_bytree=0.8, learning_rate=0.2,
                       max_depth=7, n_estimators=300,
                       subsample=1.0, random_state=42)
    metrics_xgb = evaluate_cv(xgb, X, y)
    metrics_xgb['Model'] = 'XGBoost'
    results.append(metrics_xgb)

    # HistGradientBoosting
    hgb = HistGradientBoostingRegressor(l2_regularization=1.0,
                                        learning_rate=0.2,
                                        max_depth=None, max_iter=200,
                                        min_samples_leaf=30,
                                        random_state=42)
    metrics_hgb = evaluate_cv(hgb, X, y)
    metrics_hgb['Model'] = 'HistGB'
    results.append(metrics_hgb)

    # Plot metrics comparison
    plot_metrics_comparison(results)

    # Feature importances on full fit for visualization
    for rec in results:
        name = rec['Model']
        model = {'CatBoost': cb ,
                 'XGBoost': xgb,
                 'HistGB': hgb}[name]
        plot_top_features(model, X.columns, f"Top Features ({name})", X, y)


if __name__ == "__main__":
    main()
