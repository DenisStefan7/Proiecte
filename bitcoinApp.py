import requests
import tkinter as tk
from tkinter import messagebox
import matplotlib.pyplot as plt
from datetime import datetime, timedelta

# Lista criptomonedelor
coins = ["bitcoin", "ethereum", "dogecoin"]

# Prag pentru alertă
PRICE_ALERT = {"bitcoin": 35000, "ethereum": 2000, "dogecoin": 0.08}

# Funcție pentru preț curent
def get_current_prices():
    url = "https://api.coingecko.com/api/v3/simple/price"
    params = {"ids": ",".join(coins), "vs_currencies": "usd"}
    response = requests.get(url, params=params).json()
    return response

# Funcție pentru grafic istoric (ultimele 7 zile)
def get_historical_prices(coin):
    end_date = datetime.now()
    start_date = end_date - timedelta(days=7)
    url = f"https://api.coingecko.com/api/v3/coins/{coin}/market_chart"
    params = {"vs_currency": "usd", "days": "7"}
    response = requests.get(url, params=params).json()
    prices = response["prices"]
    dates = [datetime.fromtimestamp(p[0]/1000).strftime("%d-%m") for p in prices]
    values = [p[1] for p in prices]
    return dates, values

# Funcție pentru actualizare GUI
def update_prices():
    data = get_current_prices()
    listbox.delete(0, tk.END)
    for coin, info in data.items():
        price = info["usd"]
        listbox.insert(tk.END, f"{coin.capitalize()}: ${price}")
        if PRICE_ALERT.get(coin) and price > PRICE_ALERT[coin]:
            messagebox.showinfo("Alertă Preț", f"{coin.capitalize()} a depășit ${PRICE_ALERT[coin]}!")
            
# Funcție pentru afișare grafic
def show_chart():
    selected = listbox.curselection()
    if not selected:
        messagebox.showwarning("Atenție", "Selectează o monedă din listă!")
        return
    coin = coins[selected[0]]
    dates, values = get_historical_prices(coin)
    plt.figure(figsize=(10,5))
    plt.plot(dates, values, marker='o')
    plt.title(f"Preț istoric {coin.capitalize()} (ultimele 7 zile)")
    plt.xlabel("Zi")
    plt.ylabel("USD")
    plt.xticks(rotation=45)
    plt.grid(True)
    plt.show()

# --- GUI ---
root = tk.Tk()
root.title("Crypto Tracker")

listbox = tk.Listbox(root, width=50, height=10)
listbox.pack(padx=10, pady=10)

btn_frame = tk.Frame(root)
btn_frame.pack(pady=5)

refresh_btn = tk.Button(btn_frame, text="Actualizează Prețuri", command=update_prices)
refresh_btn.grid(row=0, column=0, padx=5)

chart_btn = tk.Button(btn_frame, text="Grafic Istoric", command=show_chart)
chart_btn.grid(row=0, column=1, padx=5)

# Primele prețuri la start
update_prices()

root.mainloop()