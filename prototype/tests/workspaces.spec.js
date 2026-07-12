import { expect, test } from "@playwright/test";

test("Inspect is the default workspace with a floating tool surface", async ({ page }) => {
  await page.goto("/");
  await expect(page.getByRole("tab", { name: "Inspect" })).toHaveAttribute("aria-selected", "true");
  for (const name of ["Fit", "Isolate", "Section", "Measure", "Ghost", "Capture"]) {
    await expect(page.getByRole("button", { name })).toBeVisible();
  }
});

test("highlight does not mutate the checked export set", async ({ page }) => {
  await page.goto("/");
  await page.getByRole("tab", { name: "Export" }).click();
  await page.getByRole("checkbox", { name: "Export Front cover" }).check();
  await page.getByRole("button", { name: /Lens guide/ }).click();
  await expect(page.getByRole("checkbox", { name: "Export Front cover" })).toBeChecked();
  await page.getByRole("tab", { name: "Inspect" }).click();
  await page.getByRole("tab", { name: "Export" }).click();
  await expect(page.getByRole("checkbox", { name: "Export Front cover" })).toBeChecked();
  await page.getByRole("button", { name: /Front cover/ }).click();
  await expect(page.getByTestId("master-highlight")).toHaveText("Front cover");
  await expect(page.getByTestId("isolated-preview")).toHaveText("Front cover");
  await expect(page.getByRole("checkbox", { name: /Export Front cover/ })).toBeChecked();
});

test("workspace tabs and component list support keyboard selection", async ({ page }) => {
  await page.goto("/");
  for (const tab of [page.getByRole("tab", { name: "Inspect" }), page.getByRole("tab", { name: "Export" })]) {
    const panelId = await tab.getAttribute("aria-controls");
    await expect(page.locator(`#${panelId}`)).toHaveCount(1);
  }
  const inspect = page.getByRole("tab", { name: "Inspect" });
  await inspect.focus();
  await page.keyboard.press("ArrowRight");
  await expect(page.getByRole("tab", { name: "Export" })).toBeFocused();
  await expect(page.getByRole("tab", { name: "Export" })).toHaveAttribute("aria-selected", "true");

  await expect(page.getByRole("listbox")).toHaveCount(0);
  const lensGuide = page.getByRole("button", { name: /Lens guide/ });
  await lensGuide.focus();
  await page.keyboard.press("Enter");
  await expect(page.getByTestId("master-highlight")).toHaveText("Lens guide");
});

test("unresolved units block export review", async ({ page }) => {
  await page.goto("/?fixture=suspicious-units");
  await page.getByRole("tab", { name: "Export" }).click();
  await expect(page.getByRole("button", { name: "Review export plan" })).toBeDisabled();
});

test("keyboard focus remains visible and reduced motion does not remove state", async ({ page }) => {
  await page.emulateMedia({ reducedMotion: "reduce" });
  await page.goto("/");
  await page.getByRole("tab", { name: "Export" }).focus();
  await expect(page.getByRole("tab", { name: "Export" })).toBeFocused();
  await expect(page.getByRole("tab", { name: "Export" })).toHaveCSS("outline-width", "3px");
  await page.keyboard.press("Enter");
  await expect(page.getByRole("tab", { name: "Export" })).toHaveAttribute("aria-selected", "true");
  const duration = await page.getByRole("tab", { name: "Export" }).evaluate((element) => getComputedStyle(element).transitionDuration);
  expect(Number.parseFloat(duration)).toBeLessThanOrEqual(0.01);
});
